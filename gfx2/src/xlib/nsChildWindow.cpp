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

#include "nsChildWindow.h"
#include "nsWindowHelper.h"

#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsComponentManagerUtils.h"


#include "nsString.h"

#include "nsRunAppRun.h"

#include <X11/Xutil.h>

NS_IMPL_QUERY_HEAD(nsChildWindow)
NS_IMPL_QUERY_BODY(nsIChildWindow)
NS_IMPL_QUERY_TAIL_INHERITING(nsWindow)

NS_IMPL_ADDREF_INHERITED(nsChildWindow, nsWindow)
NS_IMPL_RELEASE_INHERITED(nsChildWindow, nsWindow)


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



nsChildWindow::nsChildWindow()
{
  NS_INIT_ISUPPORTS();

  mDisplay = nsRunAppRun::sDisplay;
}

nsChildWindow::~nsChildWindow()
{
  SetDrawable((Drawable)0);

  if (mWindow) {
    nsWindowHelper::RemoveWindow(mWindow);
    ::XDestroyWindow(mDisplay, mWindow);
  }
}

/* nsIChildWindow methods */
NS_IMETHODIMP nsChildWindow::Init(nsIWindow *aParent,
                                  gfx_coord aX,
                                  gfx_coord aY,
                                  gfx_dimension aWidth,
                                  gfx_dimension aHeight)
{

  if (!aParent) {
    printf("no parent window\n");
    return NS_ERROR_FAILURE;
  }

  mBounds.SetRect(aX, aY, aWidth, aHeight);
  mDepth = 24; // this could be wrong :)

  Window parent = 0;

  mParent = getter_AddRefs(NS_GetWeakReference(aParent));

  nsCOMPtr<nsPIWindowXlib> wx(do_QueryInterface(aParent));
  if (!wx)
    return NS_ERROR_FAILURE;

  wx->GetNativeWindow(&parent);

  XSetWindowAttributes attr;
  unsigned long attr_mask;

  Screen *screen = DefaultScreenOfDisplay(mDisplay);

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


/* void takeFocus (); */
NS_IMETHODIMP nsChildWindow::TakeFocus()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void moveZOrderAboveSibling (in nsIChildWindow aSibling); */
NS_IMETHODIMP nsChildWindow::MoveZOrderAboveSibling(nsIChildWindow *aSibling)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void moveZOrderBelowSibling (in nsIChildWindow aSibling); */
NS_IMETHODIMP nsChildWindow::MoveZOrderBelowSibling(nsIChildWindow *aSibling)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

