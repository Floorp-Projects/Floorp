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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsPopupWindow.h"
#include "nsWindowHelper.h"

#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsComponentManagerUtils.h"


#include "nsString.h"

#include "nsRunAppRun.h"

#include <X11/Xutil.h>

NS_IMPL_QUERY_HEAD(nsPopupWindow)
NS_IMPL_QUERY_BODY(nsIPopupWindow)
NS_IMPL_QUERY_TAIL_INHERITING(nsWindow)

NS_IMPL_ADDREF_INHERITED(nsPopupWindow, nsWindow)
NS_IMPL_RELEASE_INHERITED(nsPopupWindow, nsWindow)


#define ALL_EVENTS ( KeyPressMask | KeyReleaseMask | ButtonPressMask | \
                     ButtonReleaseMask | EnterWindowMask | LeaveWindowMask | \
                     PointerMotionMask | PointerMotionHintMask | Button1MotionMask | \
                     Button2MotionMask | Button3MotionMask | \
                     Button4MotionMask | Button5MotionMask | ButtonMotionMask | \
                     KeymapStateMask | ExposureMask | VisibilityChangeMask | \
                     StructureNotifyMask | ResizeRedirectMask | \
                     SubstructureNotifyMask | SubstructureRedirectMask | \
                     FocusChangeMask | PropertyChangeMask | \
                     ColormapChangeMask | OwnerGrabButtonMask )



nsPopupWindow::nsPopupWindow()
{
  NS_INIT_ISUPPORTS();

  mDisplay = nsRunAppRun::sDisplay;
}

nsPopupWindow::~nsPopupWindow()
{
  SetDrawable((Drawable)0);

  if (mWindow) {
    nsWindowHelper::RemoveWindow(mWindow);
    ::XDestroyWindow(mDisplay, mWindow);
  }
}

/* nsIPopupWindow methods */
NS_IMETHODIMP nsPopupWindow::Init(nsIWindow *aParent,
                                  gfx_coord aX,
                                  gfx_coord aY,
                                  gfx_dimension aWidth,
                                  gfx_dimension aHeight)
{
  mBounds.SetRect(aX, aY, aWidth, aHeight);

  mDepth = 24; // this could be wrong :)

  Window parent = 0;

  Screen *screen = DefaultScreenOfDisplay(mDisplay);

  if (aParent) {
    mParent = getter_AddRefs(NS_GetWeakReference(aParent));

    nsCOMPtr<nsPIWindowXlib> wx(do_QueryInterface(aParent));
    if (!wx)
      return NS_ERROR_FAILURE;

    wx->GetNativeWindow(&parent);
  } else {
    parent = RootWindowOfScreen(screen);
  }

  XSetWindowAttributes attr;
  unsigned long attr_mask;

  attr.bit_gravity = NorthWestGravity;
  attr.colormap = DefaultColormapOfScreen(screen);
  attr.event_mask = ALL_EVENTS;
  attr_mask = CWBitGravity | CWColormap | CWEventMask;

  mWindow = ::XCreateWindow(mDisplay, parent, aX, aY,
                            aWidth, aHeight,
                            0,
                            CopyFromParent, // is this what we want here for depth?
                            InputOutput,
                            DefaultVisualOfScreen(screen),
                            attr_mask, &attr);

  nsWindowHelper::AddWindow(mWindow, this);

  SetDrawable((Drawable)mWindow);

  return NS_OK;
}


/* void bringToTop (); */
NS_IMETHODIMP nsPopupWindow::BringToTop()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setTransparency (in nsIPixmap mask); */
NS_IMETHODIMP nsPopupWindow::SetTransparency(nsIPixmap *mask)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
