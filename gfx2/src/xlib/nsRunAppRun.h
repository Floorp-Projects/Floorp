#include "nsIRunAppRun.h"

#include <X11/Xlib.h>

class nsWindow;

#define NS_RUNAPPRUN_CID \
{ /* 180455dc-1dd2-11b2-a8de-e81542e76a80 */         \
     0x180455dc,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0xa8, 0xde, 0xe8, 0x15, 0x42, 0xe7, 0x6a, 0x80} \
}

class nsRunAppRun : public nsIRunAppRun
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNAPPRUN

  nsRunAppRun();
  virtual ~nsRunAppRun();
  /* additional members */

  static Display *sDisplay;

private:
  void HandleXEvent(const XEvent *event);
  void HandleMotionNotifyEvent(const XEvent *event, nsWindow *aWindow);
  void HandleButtonEvent(const XEvent *event, nsWindow *aWindow);
  void HandleExposeEvent(const XEvent *event, nsWindow *aWindow);
  void HandleConfigureNotifyEvent(const XEvent *event, nsWindow *aWindow);
  void HandleKeyPressEvent(const XEvent *event, nsWindow *aWindow);
  void HandleKeyReleaseEvent(const XEvent *event, nsWindow *aWindow);
  void HandleFocusInEvent(const XEvent *event, nsWindow *aWindow);
  void HandleFocusOutEvent(const XEvent *event, nsWindow *aWindow);
  void HandleEnterEvent(const XEvent *event, nsWindow *aWindow);
  void HandleLeaveEvent(const XEvent *event, nsWindow *aWindow);
  void HandleVisibilityNotifyEvent(const XEvent *event, nsWindow *aWindow);
  void HandleMapNotifyEvent(const XEvent *event, nsWindow *aWindow);
  void HandleUnmapNotifyEvent(const XEvent *event, nsWindow *aWindow);
  void HandleClientMessageEvent(const XEvent *event, nsWindow *aWindow);
  void HandleSelectionRequestEvent(const XEvent *event, nsWindow *aWindow);
  void HandleDragMotionEvent(const XEvent *event, nsWindow *aWindow);
  void HandleDragEnterEvent(const XEvent *event, nsWindow *aWindow);
  void HandleDragLeaveEvent(const XEvent *event, nsWindow *aWindow);
  void HandleDragDropEvent(const XEvent *event, nsWindow *aWindow);


  static PRBool DieAppShellDie;
  static PRBool mClicked;
  static PRTime mClickTime;
  static PRInt16 mClicks;
  static PRUint16 mClickedButton;
  static Display * mDisplay;
  static PRBool mDragging;
  static PRBool mAltDown;
  static PRBool mShiftDown;
  static PRBool mCtrlDown;
  static PRBool mMetaDown;
};
