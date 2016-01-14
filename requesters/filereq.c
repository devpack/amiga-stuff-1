#include <intuition/intuition.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_stdio_protos.h>

#include "filereq.h"

#define REQWIN_WIDTH 260
#define REQWIN_HEIGHT 180
#define REQ_X 10
#define REQ_Y 16
#define REQ_WIDTH 240
#define REQ_HEIGHT 175
#define TOPAZ_BASELINE 8

#define BUTTON_Y      135
#define BUTTON_HEIGHT 18

#define BUTTON_TEXT_XOFFSET  5
#define BUTTON_TEXT_YOFFSET  (TOPAZ_BASELINE - 3)

#define OK_BUTTON_X          0
#define OK_BUTTON_WIDTH      40
#define DRIVES_BUTTON_X      46
#define DRIVES_BUTTON_WIDTH  58
#define PARENT_BUTTON_X      110
#define PARENT_BUTTON_WIDTH  58
#define CANCEL_BUTTON_X      174
#define CANCEL_BUTTON_WIDTH  58

#define DRAWER_GADGET_X  60
#define DRAWER_GADGET_Y  98
#define FILE_GADGET_X  DRAWER_GADGET_X
#define FILE_GADGET_Y  115
#define PATH_GADGET_WIDTH    160
#define STR_LABEL_XOFFSET -60
#define STR_LABEL_YOFFSET 2

#define FILE_LIST_WIDTH 224
#define FILE_LIST_HEIGHT 86

#define LIST_VSLIDER_X FILE_LIST_WIDTH
#define LIST_VSLIDER_Y 0
#define LIST_VSLIDER_WIDTH 12
#define LIST_VSLIDER_HEIGHT 70

#define REQ_OK_BUTTON_ID     101
#define REQ_DRIVES_BUTTON_ID 102
#define REQ_PARENT_BUTTON_ID 103
#define REQ_CANCEL_BUTTON_ID 104

static struct Requester requester;
static BOOL req_opened = FALSE;
static struct Window *req_window;

static struct IntuiText drawer_str_label =  {1, 0, JAM2, STR_LABEL_XOFFSET, STR_LABEL_YOFFSET, NULL,
                                             "Drawer", NULL};
static struct IntuiText file_str_label = {1, 0, JAM2, STR_LABEL_XOFFSET, STR_LABEL_YOFFSET, NULL,
                                              "File", NULL};
static struct IntuiText parent_button_label = {1, 0, JAM2, BUTTON_TEXT_XOFFSET, BUTTON_TEXT_YOFFSET, NULL, "Parent", NULL};
static struct IntuiText drives_button_label = {1, 0, JAM2, BUTTON_TEXT_XOFFSET, BUTTON_TEXT_YOFFSET, NULL, "Drives", NULL};
static struct IntuiText ok_button_label = {1, 0, JAM2, BUTTON_TEXT_XOFFSET, BUTTON_TEXT_YOFFSET, NULL, "Open", NULL};
static struct IntuiText cancel_button_label = {1, 0, JAM2, BUTTON_TEXT_XOFFSET, BUTTON_TEXT_YOFFSET, NULL,
                                               "Cancel", NULL};

static WORD ok_border_points[] = {0, 0, OK_BUTTON_WIDTH, 0, OK_BUTTON_WIDTH, BUTTON_HEIGHT, 0,
                                  BUTTON_HEIGHT, 0, 0};
static WORD drives_border_points[] = {0, 0, DRIVES_BUTTON_WIDTH, 0, DRIVES_BUTTON_WIDTH,
                                      BUTTON_HEIGHT, 0, BUTTON_HEIGHT, 0, 0};

static WORD cancel_border_points[] = {0, 0, CANCEL_BUTTON_WIDTH, 0, CANCEL_BUTTON_WIDTH,
                                      BUTTON_HEIGHT, 0, BUTTON_HEIGHT, 0, 0};
static WORD list_border_points[] = {0, 0, FILE_LIST_WIDTH, 0, FILE_LIST_WIDTH,
                                    FILE_LIST_HEIGHT, 0, FILE_LIST_HEIGHT, 0, 0};

/* the -2 is the margin to set to avoid that the string gadget will overdraw the
   borders */
static WORD string_border_points[] =  {-2, -2, PATH_GADGET_WIDTH, -2, PATH_GADGET_WIDTH,
                                       10, -2, 10, -2, -2};

static struct Border ok_button_border = {0, 0, 1, 0, JAM1, 5, ok_border_points, NULL};
static struct Border drives_button_border = {0, 0, 1, 0, JAM1, 5, cancel_border_points, NULL};
static struct Border parent_button_border = {0, 0, 1, 0, JAM1, 5, cancel_border_points, NULL};
static struct Border cancel_button_border = {0, 0, 1, 0, JAM1, 5, cancel_border_points, NULL};
static struct Border str_gadget_border = {0, 0, 1, 0, JAM1, 5, string_border_points, NULL};
static struct Border file_list_border = {0, 0, 1, 0, JAM1, 5, list_border_points, NULL};

static UBYTE buffer1[82], undobuffer1[82];
static struct StringInfo strinfo1 = {buffer1, undobuffer1, 0, 80, 0, 0, 0, 0, 0, 0, NULL, 0, NULL};
static UBYTE buffer2[82], undobuffer2[82];
static struct StringInfo strinfo2 = {buffer2, undobuffer2, 0, 80, 0, 0, 0, 0, 0, 0, NULL, 0, NULL};

static struct Image slider_image;
static struct PropInfo propinfo = {AUTOKNOB | FREEVERT, 0, 0, MAXBODY, MAXBODY,
                                   0, 0, 0, 0, 0, 0};

#define WIN_TITLE "Open File..."

static struct NewWindow newwin = {
  0, 0, REQWIN_WIDTH, REQWIN_HEIGHT, 0, 1,
  IDCMP_GADGETUP,
  WFLG_CLOSEGADGET | WFLG_SMART_REFRESH | WFLG_ACTIVATE | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_NOCAREREFRESH,
  NULL, NULL, WIN_TITLE,
  NULL, NULL,
  REQWIN_WIDTH, REQWIN_HEIGHT,
  REQWIN_WIDTH, REQWIN_HEIGHT,
  WBENCHSCREEN
};

static struct Gadget list_vslider = {NULL, LIST_VSLIDER_X, LIST_VSLIDER_Y,
                                     LIST_VSLIDER_WIDTH, LIST_VSLIDER_HEIGHT,
                                     GFLG_GADGHCOMP, GACT_RELVERIFY, GTYP_PROPGADGET,
                                     &slider_image, NULL, NULL, 0, &propinfo, 112, NULL};

static struct Gadget file_text = {&list_vslider, FILE_GADGET_X, FILE_GADGET_Y, PATH_GADGET_WIDTH, 10,
                                  GFLG_GADGHCOMP, GACT_RELVERIFY, GTYP_STRGADGET, &str_gadget_border,
                                  NULL, &file_str_label, 0, &strinfo2, 111, NULL};

static struct Gadget dir_text = {&file_text, DRAWER_GADGET_X, DRAWER_GADGET_Y, PATH_GADGET_WIDTH, 10,
                                 GFLG_GADGHCOMP, GACT_RELVERIFY, GTYP_STRGADGET, &str_gadget_border,
                                 NULL, &drawer_str_label, 0, &strinfo1, 110, NULL};
/*
  Note: Cancel does not specify the GACT_ENDGADGET flag, it seems that
  IDCMP_REQCLEAR is not fired when Intuition closes the requester automatically
*/
static struct Gadget cancel_button = {&dir_text, CANCEL_BUTTON_X, BUTTON_Y, CANCEL_BUTTON_WIDTH,
                                      BUTTON_HEIGHT, GFLG_GADGHCOMP, GACT_RELVERIFY,
                                      GTYP_BOOLGADGET | GTYP_REQGADGET, &cancel_button_border, NULL,
                                      &cancel_button_label, 0, NULL, REQ_CANCEL_BUTTON_ID, NULL};

static struct Gadget parent_button = {&cancel_button, PARENT_BUTTON_X, BUTTON_Y, PARENT_BUTTON_WIDTH,
                                      BUTTON_HEIGHT,
                                      GFLG_GADGHCOMP, GACT_RELVERIFY, GTYP_BOOLGADGET | GTYP_REQGADGET,
                                      &parent_button_border, NULL, &parent_button_label, 0, NULL,
                                      REQ_PARENT_BUTTON_ID, NULL};

static struct Gadget drives_button = {&parent_button, DRIVES_BUTTON_X, BUTTON_Y, DRIVES_BUTTON_WIDTH,
                                      BUTTON_HEIGHT,
                                      GFLG_GADGHCOMP, GACT_RELVERIFY, GTYP_BOOLGADGET | GTYP_REQGADGET,
                                      &drives_button_border, NULL, &drives_button_label, 0, NULL,
                                      REQ_DRIVES_BUTTON_ID, NULL};

static struct Gadget ok_button = {&drives_button, OK_BUTTON_X, BUTTON_Y, OK_BUTTON_WIDTH, BUTTON_HEIGHT,
                                  GFLG_GADGHCOMP, GACT_RELVERIFY, GTYP_BOOLGADGET | GTYP_REQGADGET,
                                  &ok_button_border, NULL, &ok_button_label, 0, NULL,
                                  REQ_OK_BUTTON_ID, NULL};

#define PATHBUFFER_SIZE 200
static char dirname[PATHBUFFER_SIZE + 1];
static BPTR flock;
static LONG error;
static struct FileInfoBlock fileinfo;

static void print_fileinfo(struct FileInfoBlock *fileinfo)
{
    if (fileinfo->fib_DirEntryType > 0) {
        printf("dir: '%s'\n", fileinfo->fib_FileName);
    } else {
        printf("file: '%s'\n", fileinfo->fib_FileName);
    }
}

static void close_requester()
{
    if (req_opened) {
        EndRequest(&requester, req_window);
        req_opened = FALSE;
    }
}

static void handle_events()
{
    BOOL done = FALSE;
    struct IntuiMessage *msg;
    ULONG msgClass;
    UWORD menuCode;
    int buttonId;

    while (!done) {
        Wait(1 << req_window->UserPort->mp_SigBit);
        if (msg = (struct IntuiMessage *) GetMsg(req_window->UserPort)) {
            msgClass = msg->Class;
            switch (msgClass) {
            case IDCMP_GADGETUP:
                buttonId = (int) ((struct Gadget *) (msg->IAddress))->GadgetID;
                ReplyMsg((struct Message *) msg);
                if (buttonId == REQ_OK_BUTTON_ID) {
                    close_requester();
                    done = TRUE;
                }
                else if (buttonId == REQ_CANCEL_BUTTON_ID) {
                    close_requester();
                    done = TRUE;
                }
                break;
            default:
                break;
            }
        }
    }
}

void open_file(struct Window *window)
{
    BOOL result;
    if (req_window = OpenWindow(&newwin)) {
        InitRequester(&requester);
        requester.LeftEdge = REQ_X;
        requester.TopEdge = REQ_Y;
        requester.Width = REQ_WIDTH;
        requester.Height = REQ_HEIGHT;
        requester.Flags = SIMPLEREQ;
        requester.BackFill = 0;
        requester.ReqGadget = &ok_button;
        requester.ReqBorder = &file_list_border;
        requester.ReqText = NULL;

        /* scan current directory */
        /*
          on AmigaOS versions before 36 (essentially all 1.x versions), the
          function GetCurrentDirName() does not exist, but it's obtainable
          by calling Cli() and querying the returned CommandLineInterface
          structure
        */
        //puts("scanning directory...");
        /*
        // on AmigaOS 1.x, this function does not exist !!!
        GetCurrentDirName(dirname, PATHBUFFER_SIZE);
        printf("current dir: '%s'\n", dirname);
        flock = Lock(dirname, SHARED_LOCK);
        if (Examine(flock, &fileinfo)) {
        while (ExNext(flock, &fileinfo)) {
        print_fileinfo(&fileinfo);
        }
        error = IoErr();
        if (error != ERROR_NO_MORE_ENTRIES) {
        puts("unknown I/O error, TODO handle");
        }
        }
        UnLock(flock);
        */
        if (req_opened = Request(&requester, req_window)) {
            handle_events();
            CloseWindow(req_window);
            req_window = NULL;
        } else {
            puts("Request() failed !!!");
        }
    } else {
        puts("OpenWindow() failed !!!");
    }
}
