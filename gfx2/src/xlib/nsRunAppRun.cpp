/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000-2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsRunAppRun.h"

#include "prlog.h"


#include "nsWindow.h"
#include "nsWindowHelper.h"


#include "nsIDragService.h"

#include "nsRegion.h"

#include "nsGUIEvent.h"

#include "nsRect2.h"
#include "nsPoint2.h"

#include "nsKeyCode.h"

#include "nsIServiceManager.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>


Display *nsRunAppRun::sDisplay = nsnull;


PRBool nsRunAppRun::DieAppShellDie = PR_FALSE;
PRBool nsRunAppRun::mClicked = PR_FALSE;
PRTime nsRunAppRun::mClickTime = 0;
PRInt16 nsRunAppRun::mClicks = 1;
PRUint16 nsRunAppRun::mClickedButton = 0;
Display *nsRunAppRun::mDisplay = nsnull;
PRBool nsRunAppRun::mDragging = PR_FALSE;
PRBool nsRunAppRun::mAltDown = PR_FALSE;
PRBool nsRunAppRun::mShiftDown = PR_FALSE;
PRBool nsRunAppRun::mCtrlDown = PR_FALSE;
PRBool nsRunAppRun::mMetaDown = PR_FALSE;

PRLogModuleInfo *XlibWidgetsLM = PR_NewLogModule("XlibWidgets");

// For debugging.
static char *event_names[] = {
  "",
  "",
  "KeyPress",
  "KeyRelease",
  "ButtonPress",
  "ButtonRelease",
  "MotionNotify",
  "EnterNotify",
  "LeaveNotify",
  "FocusIn",
  "FocusOut",
  "KeymapNotify",
  "Expose",
  "GraphicsExpose",
  "NoExpose",
  "VisibilityNotify",
  "CreateNotify",
  "DestroyNotify",
  "UnmapNotify",
  "MapNotify",
  "MapRequest",
  "ReparentNotify",
  "ConfigureNotify",
  "ConfigureRequest",
  "GravityNotify",
  "ResizeRequest",
  "CirculateNotify",
  "CirculateRequest",
  "PropertyNotify",
  "SelectionClear",
  "SelectionRequest",
  "SelectionNotify",
  "ColormapNotify",
  "ClientMessage",
  "MappingNotify"
};



NS_IMPL_ISUPPORTS1(nsRunAppRun, nsIRunAppRun)


static int
my_x_error (Display     *display,
             XErrorEvent *error)
{
  return 1;
}

nsRunAppRun::nsRunAppRun()
{
  NS_INIT_ISUPPORTS();

  sDisplay = ::XOpenDisplay(NULL);


  //  XSetErrorHandler(my_x_error);

  /* member initializers and constructor code */
}

nsRunAppRun::~nsRunAppRun()
{
  ::XCloseDisplay(sDisplay);
  /* destructor code */
}

/* void go (); */
NS_IMETHODIMP nsRunAppRun::Go()
{
  while(1) {

    XEvent event;
    XNextEvent(sDisplay, &event);

    HandleXEvent(&event);

  }
  return NS_OK;
}


void
nsRunAppRun::HandleXEvent(const XEvent *event)
{

  nsWindow *window = (nsWindow*)nsWindowHelper::FindIWindow(event->xany.window);

  // did someone pass us an x event for a window we don't own?
  // bad! bad!
  if (window == nsnull)
    return;

  // switch on the type of event
  switch (event->type) 
  {
  case Expose:
    HandleExposeEvent(event, window);
    break;

  case ConfigureNotify:
    // we need to make sure that this is the LAST of the
    // config events.
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("DispatchEvent: ConfigureNotify event for window 0x%lx %d %d %d %d\n",
                                         event->xconfigure.window,
                                         event->xconfigure.x, 
                                         event->xconfigure.y,
                                         event->xconfigure.width, 
                                         event->xconfigure.height));

    HandleConfigureNotifyEvent(event, window);

    break;

  case ButtonPress:
  case ButtonRelease:
    HandleFocusInEvent(event, window);
    HandleButtonEvent(event, window);
    break;

  case MotionNotify:
    HandleMotionNotifyEvent(event, window);
    break;

  case KeyPress:
    HandleKeyPressEvent(event, window);
    break;
  case KeyRelease:
    HandleKeyReleaseEvent(event, window);
    break;

  case FocusIn:
    HandleFocusInEvent(event, window);
    break;

  case FocusOut:
    HandleFocusOutEvent(event, window);
    break;

  case EnterNotify:
    HandleEnterEvent(event, window);
    break;

  case LeaveNotify:
    HandleLeaveEvent(event, window);
    break;

  case NoExpose:
    // these annoy me.
    break;
  case VisibilityNotify:
    HandleVisibilityNotifyEvent(event, window);
    break;
  case ClientMessage:
    HandleClientMessageEvent(event, window);
    break;
  case SelectionRequest:
    HandleSelectionRequestEvent(event, window);
    break;
  default:
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Unhandled window event: Window 0x%lx Got a %s event\n",
                                         event->xany.window, event_names[event->type]));
    break;
  }
}

void
nsRunAppRun::HandleMotionNotifyEvent(const XEvent *aEvent, nsWindow *aWindow)
{
  nsMouseEvent mevent;

  if (mDragging) {
    HandleDragMotionEvent(aEvent, aWindow);
  }

  mevent.window = aWindow;
  mevent.time = 0;
  mevent.point.x = aEvent->xmotion.x;
  mevent.point.y = aEvent->xmotion.y;

  //  Display * dpy = (Display *)aWindow->GetNativeData(NS_NATIVE_DISPLAY);
  Display *dpy = sDisplay;
  Window win;
  aWindow->GetNativeWindow(&win);

  XEvent event;
  // We are only interested in the LAST (newest) location of the pointer
  while(XCheckWindowEvent(dpy,
                          win,
                          ButtonMotionMask,
                          &event)) {
    mevent.point.x = event.xmotion.x;
    mevent.point.y = event.xmotion.y;
  }

  mevent.message = NS_MOUSE_MOVE;
  mevent.eventStructType = NS_MOUSE_EVENT;

  aWindow->DispatchEvent(&mevent);
}

void
nsRunAppRun::HandleButtonEvent(const XEvent *event, nsWindow *aWindow)
{
  nsMouseEvent mevent;
  mevent.isShift = mShiftDown;
  mevent.isControl = mCtrlDown;
  mevent.isAlt = mAltDown;
  mevent.isMeta = mMetaDown;
  PRUint32 eventType = 0;
  PRBool currentlyDragging = mDragging;
  nsMouseScrollEvent scrollEvent;

  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Button event for window 0x%lx button %d type %s\n",
                                       event->xany.window,
                                       event->xbutton.button,
                                       (event->type == ButtonPress ? "ButtonPress" : "ButtonRelease")));

  switch(event->type) {
  case ButtonPress:
    switch(event->xbutton.button) {
    case 1:
      eventType = NS_MOUSE_LEFT_BUTTON_DOWN;
      mDragging = PR_TRUE;
      break;
    case 2:
      eventType = NS_MOUSE_MIDDLE_BUTTON_DOWN;
      break;
    case 3:
      eventType = NS_MOUSE_RIGHT_BUTTON_DOWN;
      break;
    case 4:
    case 5:
      scrollEvent.deltaLines = (event->xbutton.button == 4) ? -3 : 3;
      scrollEvent.message = NS_MOUSE_SCROLL;
      scrollEvent.window = aWindow;
      scrollEvent.eventStructType = NS_MOUSE_SCROLL_EVENT;

      scrollEvent.point.x = event->xbutton.x;
      scrollEvent.point.y = event->xbutton.y;

      scrollEvent.isShift = mShiftDown;
      scrollEvent.isControl = mCtrlDown;
      scrollEvent.isAlt = mAltDown;
      scrollEvent.isMeta = mMetaDown;
      scrollEvent.time = PR_Now();
      NS_IF_ADDREF(aWindow);
      aWindow->DispatchEvent(&scrollEvent);
      NS_IF_RELEASE(aWindow);
      return;
    }
    break;
  case ButtonRelease:
    switch(event->xbutton.button) {
    case 1:
      eventType = NS_MOUSE_LEFT_BUTTON_UP;
      mDragging = PR_FALSE;
      break;
    case 2:
      eventType = NS_MOUSE_MIDDLE_BUTTON_UP;
      break;
    case 3:
      eventType = NS_MOUSE_RIGHT_BUTTON_UP;
      break;
    case 4:
    case 5:
      return;
    }
    break;
  }

  mevent.window = aWindow;
  mevent.point.x = event->xbutton.x;
  mevent.point.y = event->xbutton.y;
  mevent.time = PR_Now();
  
  // If we are waiting longer than 1 sec for the second click, this is not a
  // double click.
  if (PR_Now() - mClickTime > 1000000)
    mClicked = PR_FALSE;               

  if (event->type == ButtonPress) {
    if (!mClicked) {
      mClicked = PR_TRUE;
      mClickTime = PR_Now();
      mClicks = 1;
      mClickedButton = event->xbutton.button;
    } else {
      mClickTime = PR_Now() - mClickTime;
      if ((mClickTime < 500000) && (mClickedButton == event->xbutton.button))
        mClicks = 2;
      else
        mClicks = 1;
      mClicked = PR_FALSE;
    }
  }

  if (currentlyDragging && !mDragging) {
    HandleDragDropEvent(event, aWindow);
  }

  mevent.message = eventType;
  mevent.eventStructType = NS_MOUSE_EVENT;
  mevent.clickCount = mClicks;

  aWindow->DispatchEvent(&mevent);
}

void
nsRunAppRun::HandleExposeEvent(const XEvent *event, nsWindow *aWindow)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Expose event for window 0x%lx %d %d %d %d\n", event->xany.window,
                                       event->xexpose.x, event->xexpose.y, event->xexpose.width, event->xexpose.height));


  nsCOMPtr<nsIRegion> region(do_CreateInstance("@mozilla.org/gfx/region;2"));
  
  region->SetToRect(event->xexpose.x, event->xexpose.y, event->xexpose.width, event->xexpose.height);

  /* compress expose events...
   */
  if (event->xexpose.count != 0) {
    XEvent txe;
    do {
      XWindowEvent(event->xany.display, event->xany.window, ExposureMask, (XEvent *)&txe);
      region->UnionRect(txe.xexpose.x, txe.xexpose.y, 
			txe.xexpose.width, txe.xexpose.height);
    } while (txe.xexpose.count > 0);
  }

  nsPaintEvent pevent;
  pevent.message = NS_PAINT;
  pevent.window = aWindow;
  pevent.eventStructType = NS_PAINT_EVENT;


  // XXX fix this
  pevent.time = 0;
  //pevent.time = PR_Now();
  //pevent.region = nsnull;
  //  pevent.rect = dirtyRect;


  //  aWindow->SetBackgroundColor(255);

  nsRect2 r;
  region->GetBoundingBox(&r.x, &r.y, &r.width, &r.height);
  aWindow->FillRectangle(r.x, r.y, r.width, r.height);

  aWindow->DispatchEvent(&pevent);

  //  delete pevent.rect;
}

void
nsRunAppRun::HandleConfigureNotifyEvent(const XEvent *aEvent, nsWindow *aWindow)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("ConfigureNotify event for window 0x%lx %d %d %d %d\n",
                                       aEvent->xconfigure.window,
                                       aEvent->xconfigure.x, aEvent->xconfigure.y,
                                       aEvent->xconfigure.width, aEvent->xconfigure.height));

  XEvent *event = (XEvent *)aEvent; // we promise not to change you
  XEvent config_event;
  while (XCheckTypedWindowEvent(aEvent->xany.display, 
                                aEvent->xany.window, 
                                ConfigureNotify,
                                &config_event) == True) {
    // make sure that we don't get other types of events.  
    // StructureNotifyMask includes other kinds of events, too.
    if (config_event.type == ConfigureNotify) {
      *event = config_event;
      PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("DispatchEvent: Extra ConfigureNotify event for window 0x%lx %d %d %d %d\n",
					   event->xconfigure.window,
					   event->xconfigure.x, 
					   event->xconfigure.y,
					   event->xconfigure.width, 
					   event->xconfigure.height));
    }
    else {
      PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("EVENT LOSSAGE\n"));
    }
  }

  nsSizeEvent sevent;
  sevent.message = NS_SIZE;
  sevent.window = aWindow;
  sevent.eventStructType = NS_SIZE_EVENT;
  sevent.windowSize = new nsRect2 (event->xconfigure.x, event->xconfigure.y,
                                  event->xconfigure.width, event->xconfigure.height);
  sevent.point.x = event->xconfigure.x;
  sevent.point.y = event->xconfigure.y;
  sevent.mWinWidth = event->xconfigure.width;
  sevent.mWinHeight = event->xconfigure.height;
  // XXX fix this
  sevent.time = 0;

  aWindow->DispatchEvent(&sevent);

  delete sevent.windowSize;
}

#define CHAR_BUF_SIZE 40

void
nsRunAppRun::HandleKeyPressEvent(const XEvent *event, nsWindow *aWindow)
{

  char      string_buf[CHAR_BUF_SIZE];
  int       len = 0;
  Window    focusWindow = 0;
  nsWindow *focusWidget = 0;

#if 0
  // figure out where the real focus should go...
  focusWindow = nsWindow::GetFocusWindow();
  if (focusWindow) {
    focusWidget = nsWindow::GetWidgetForWindow(focusWindow);
  }
#endif
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("KeyPress event for window 0x%lx ( 0x%lx focus window )\n",
                                       event->xkey.window,
                                       focusWindow));

  // don't even bother...
  if (focusWidget == 0) {
    return;
  }

  KeySym     keysym = nsKeyCode::ConvertKeyCodeToKeySym(event->xkey.display,
                                                        event->xkey.keycode);

  switch (keysym) {
    case XK_Alt_L:
    case XK_Alt_R:
      mAltDown = PR_TRUE;
      break;
    case XK_Control_L:
    case XK_Control_R:
      mCtrlDown = PR_TRUE;
      break;
    case XK_Shift_L:
    case XK_Shift_R:
      mShiftDown = PR_TRUE;
      break;
    case XK_Meta_L:
    case XK_Meta_R:
      mMetaDown = PR_TRUE;
      break;
    default:
      break;
  }

  // Dont dispatch events for modifier keys pressed ALONE
  if (nsKeyCode::KeyCodeIsModifier(event->xkey.keycode))
  {
    return;
  }

  nsKeyEvent keyEvent;

  XComposeStatus compose;

  len = XLookupString((XKeyEvent*)&event->xkey, string_buf, CHAR_BUF_SIZE, &keysym, &compose);
  string_buf[len] = '\0';

  keyEvent.keyCode = nsKeyCode::ConvertKeySymToVirtualKey(keysym);
  keyEvent.charCode = 0;
  keyEvent.time = event->xkey.time;
  keyEvent.isShift = event->xkey.state & ShiftMask;
  keyEvent.isControl = (event->xkey.state & ControlMask) ? 1 : 0;
  keyEvent.isAlt = (event->xkey.state & Mod1Mask) ? 1 : 0;
  // I think 'meta' is the same as 'alt' in X11. Is this OK for other systems?
  keyEvent.isMeta = (event->xkey.state & Mod1Mask) ? 1 : 0;
  keyEvent.point.x = 0;
  keyEvent.point.y = 0;
  keyEvent.message = NS_KEY_DOWN;
  keyEvent.window = focusWidget;
  keyEvent.eventStructType = NS_KEY_EVENT;

  //  printf("keysym = %x, keycode = %x, vk = %x\n",
  //         keysym,
  //         event->xkey.keycode,
  //         keyEvent.keyCode);

  focusWidget->DispatchEvent(&keyEvent);

  keyEvent.keyCode = nsKeyCode::ConvertKeySymToVirtualKey(keysym);
  keyEvent.charCode = isprint(string_buf[0]) ? string_buf[0] : 0;
  keyEvent.time = event->xkey.time;
  keyEvent.isShift = event->xkey.state & ShiftMask;
  keyEvent.isControl = (event->xkey.state & ControlMask) ? 1 : 0;
  keyEvent.isAlt = (event->xkey.state & Mod1Mask) ? 1 : 0;
  keyEvent.isMeta = (event->xkey.state & Mod1Mask) ? 1 : 0;
  keyEvent.point.x = 0;
  keyEvent.point.y = 0;
  keyEvent.message = NS_KEY_PRESS;
  keyEvent.window = focusWidget;
  keyEvent.eventStructType = NS_KEY_EVENT;

  focusWidget->DispatchEvent(&keyEvent);
}

void
nsRunAppRun::HandleKeyReleaseEvent(const XEvent *event, nsWindow *aWindow)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("KeyRelease event for window 0x%lx\n",
                                       event->xkey.window));

  KeySym keysym = nsKeyCode::ConvertKeyCodeToKeySym(event->xkey.display,
						    event->xkey.keycode);

  switch (keysym) {
    case XK_Alt_L:
    case XK_Alt_R:
      mAltDown = PR_FALSE;
      break;
    case XK_Control_L:
    case XK_Control_R:
      mCtrlDown = PR_FALSE;
      break;
    case XK_Shift_L:
    case XK_Shift_R:
      mShiftDown = PR_FALSE;
      break;
    case XK_Meta_L:
    case XK_Meta_R:
      mMetaDown = PR_FALSE;
      break;
    default:
      break;
  }

  // Dont dispatch events for modifier keys pressed ALONE
  if (nsKeyCode::KeyCodeIsModifier(event->xkey.keycode))
  {
    return;
  }

  nsKeyEvent keyEvent;

  keyEvent.keyCode = nsKeyCode::ConvertKeySymToVirtualKey(keysym);
  keyEvent.charCode = 0;
  keyEvent.time = event->xkey.time;
  keyEvent.isShift = event->xkey.state & ShiftMask;
  keyEvent.isControl = (event->xkey.state & ControlMask) ? 1 : 0;
  keyEvent.isAlt = (event->xkey.state & Mod1Mask) ? 1 : 0;
  keyEvent.isMeta = (event->xkey.state & Mod1Mask) ? 1 : 0;
  keyEvent.point.x = event->xkey.x;
  keyEvent.point.y = event->xkey.y;
  keyEvent.message = NS_KEY_UP;
  keyEvent.window = aWindow;
  keyEvent.eventStructType = NS_KEY_EVENT;

  aWindow->DispatchEvent(&keyEvent);
}

void
nsRunAppRun::HandleFocusInEvent(const XEvent *event, nsWindow *aWindow)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("FocusIn event for window 0x%lx\n",
                                       event->xfocus.window));
  nsGUIEvent focusEvent;
  
  focusEvent.message = NS_GOTFOCUS;
  focusEvent.window  = aWindow;
  
  focusEvent.eventStructType = NS_GUI_EVENT;
  
  focusEvent.time = 0;
  focusEvent.point.x = 0;
  focusEvent.point.y = 0;
  
  // FIXME FIXME FIXME
  // We now have to tell the widget that is has focus here, this was
  // Previously done in cross platform code.
  // FIXME FIXME FIXME


  //  aWindow->SetFocus();
  aWindow->DispatchEvent(&focusEvent);
}

void
nsRunAppRun::HandleFocusOutEvent(const XEvent *event, nsWindow *aWindow)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("FocusOut event for window 0x%lx\n",
                                       event->xfocus.window));
  nsGUIEvent focusEvent;
  
  focusEvent.message = NS_DEACTIVATE;
  focusEvent.window  = aWindow;
  
  focusEvent.eventStructType = NS_GUI_EVENT;
  
  focusEvent.time = 0;
  focusEvent.point.x = 0;
  focusEvent.point.y = 0;
  
  aWindow->DispatchEvent(&focusEvent);
}

void
nsRunAppRun::HandleEnterEvent(const XEvent *event, nsWindow *aWindow)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Enter event for window 0x%lx\n",
                                       event->xcrossing.window));
  nsMouseEvent enterEvent;

  if (mDragging) {
    HandleDragEnterEvent(event, aWindow);
  }

  enterEvent.window  = aWindow;
  
  enterEvent.time = event->xcrossing.time;
  enterEvent.point.x = gfx_coord(event->xcrossing.x);
  enterEvent.point.y = gfx_coord(event->xcrossing.y);
  
  enterEvent.message = NS_MOUSE_ENTER;
  enterEvent.eventStructType = NS_MOUSE_EVENT;
  
  aWindow->DispatchEvent(&enterEvent);
}

void
nsRunAppRun::HandleLeaveEvent(const XEvent *event, nsWindow *aWindow)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Leave event for window 0x%lx\n",
                                       event->xcrossing.window));

  nsMouseEvent leaveEvent;
  
  if (mDragging) {
    HandleDragLeaveEvent(event, aWindow);
  }

  leaveEvent.window  = aWindow;
  
  leaveEvent.time = event->xcrossing.time;
  leaveEvent.point.x = gfx_coord(event->xcrossing.x);
  leaveEvent.point.y = gfx_coord(event->xcrossing.y);
  
  leaveEvent.message = NS_MOUSE_EXIT;
  leaveEvent.eventStructType = NS_MOUSE_EVENT;
  
  aWindow->DispatchEvent(&leaveEvent);
}

void nsRunAppRun::HandleVisibilityNotifyEvent(const XEvent *event, nsWindow *aWindow)
{
#ifdef DEBUG
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("VisibilityNotify event for window 0x%lx ",
                                       event->xfocus.window));
  switch(event->xvisibility.state) {
  case VisibilityFullyObscured:
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Fully Obscured\n"));
    break;
  case VisibilityPartiallyObscured:
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Partially Obscured\n"));
    break;
  case VisibilityUnobscured:
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Unobscured\n"));
  }
#endif
  //  aWindow->SetVisibility(event->xvisibility.state);
}

void nsRunAppRun::HandleMapNotifyEvent(const XEvent *event, nsWindow *aWindow)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("MapNotify event for window 0x%lx\n",
                                       event->xmap.window));
  // XXX set map status is gone now..
  //  aWindow->SetMapStatus(PR_TRUE);
}

void nsRunAppRun::HandleUnmapNotifyEvent(const XEvent *event, nsWindow *aWindow)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("UnmapNotifyEvent for window 0x%lx\n",
                                       event->xunmap.window));
  // XXX set map status is gone now..
  //aWindow->SetMapStatus(PR_FALSE);
}

void nsRunAppRun::HandleClientMessageEvent(const XEvent *event, nsWindow *aWindow)
{

#if 0
  // check to see if it's a WM_DELETE message
  printf("handling client message\n");
  if (nsWindow::WMProtocolsInitialized) {
    if ((Atom)event->xclient.data.l[0] == nsWindow::WMDeleteWindow) {
      printf("got a delete window event\n");
      aWindow->OnDeleteWindow();
    }
  }
#endif
}

void nsRunAppRun::HandleSelectionRequestEvent(const XEvent *event, nsWindow *aWindow)
{
  nsGUIEvent ev;

  ev.window = (nsIWindow *)aWindow;
  ev.nativeMsg = (void *)event;

  aWindow->DispatchEvent(&ev);
}

void nsRunAppRun::HandleDragMotionEvent(const XEvent *event, nsWindow *aWindow)
{
  nsresult rv;
  PRBool currentlyDragging = PR_FALSE;


#if 0
  nsCOMPtr<nsIDragService> dragService = do_GetService("component://netscape/widget/dragservice", &rv);
  nsCOMPtr<nsIDragSessionXlib> dragServiceXlib;
  if (NS_SUCCEEDED(rv)) {
    dragServiceXlib = do_QueryInterface(dragService);
    if (dragServiceXlib) {
      dragServiceXlib->IsDragging(&currentlyDragging);
    }
  }

  if (currentlyDragging) {
    nsMouseEvent mevent;
    dragServiceXlib->UpdatePosition(event->xmotion.x, event->xmotion.y);
    mevent.window = aWindow;
    mevent.point.x = event->xmotion.x;
    mevent.point.y = event->xmotion.y;

    mevent.message = NS_DRAGDROP_OVER;
    mevent.eventStructType = NS_DRAGDROP_EVENT;

    aWindow->DispatchEvent(&mevent);
  }
#endif
}

void nsRunAppRun::HandleDragEnterEvent(const XEvent *event, nsWindow *aWindow)
{
  nsresult rv;
  PRBool currentlyDragging = PR_FALSE;

  nsCOMPtr<nsIDragService> dragService = do_GetService("component://netscape/widget/dragservice", &rv);

#if 0
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDragSessionXlib> dragServiceXlib;
    dragServiceXlib = do_QueryInterface(dragService);
    if (dragServiceXlib) {
      dragServiceXlib->IsDragging(&currentlyDragging);
    }
  }

  if (currentlyDragging) {
    nsMouseEvent enterEvent;
    enterEvent.window  = aWindow;
  
    enterEvent.point.x = event->xcrossing.x;
    enterEvent.point.y = event->xcrossing.y;
  
    enterEvent.message = NS_DRAGDROP_ENTER;
    enterEvent.eventStructType = NS_DRAGDROP_EVENT;

    aWindow->DispatchEvent(&enterEvent);
  }
#endif
}

void nsRunAppRun::HandleDragLeaveEvent(const XEvent *event, nsWindow *aWindow)
{
  nsresult rv;
  PRBool currentlyDragging = PR_FALSE;
  
  nsCOMPtr<nsIDragService> dragService = do_GetService("component://netscape/widget/dragservice", &rv);

#if 0
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDragSessionXlib> dragServiceXlib;
    dragServiceXlib = do_QueryInterface(dragService);
    if (dragServiceXlib) {
      dragServiceXlib->IsDragging(&currentlyDragging);
    }
  }

  if (currentlyDragging) {
    nsMouseEvent leaveEvent;
    leaveEvent.window  = aWindow;
  
    leaveEvent.point.x = event->xcrossing.x;
    leaveEvent.point.y = event->xcrossing.y;
  
    leaveEvent.message = NS_DRAGDROP_EXIT;
    leaveEvent.eventStructType = NS_DRAGDROP_EVENT;

    aWindow->DispatchEvent(&leaveEvent);
  }
#endif
}

void nsRunAppRun::HandleDragDropEvent(const XEvent *event, nsWindow *aWindow)
{
  nsresult rv;
  PRBool currentlyDragging = PR_FALSE;

  nsCOMPtr<nsIDragService> dragService = do_GetService("component://netscape/widget/dragservice", &rv);

#if 0
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDragSessionXlib> dragServiceXlib;
    dragServiceXlib = do_QueryInterface(dragService);
    if (dragServiceXlib) {
      dragServiceXlib->IsDragging(&currentlyDragging);
    }
  }

  if (currentlyDragging) {
    nsMouseEvent mevent;
    mevent.window = aWindow;
    mevent.point.x = event->xbutton.x;
    mevent.point.y = event->xbutton.y;
  
    mevent.message = NS_DRAGDROP_DROP;
    mevent.eventStructType = NS_DRAGDROP_EVENT;

    aWindow->DispatchEvent(&mevent);
  }
#endif
}
