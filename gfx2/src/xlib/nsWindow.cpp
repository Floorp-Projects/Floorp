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

#include "nsWindow.h"
#include "nsWindowHelper.h"

#include "nsCursor.h"

#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsComponentManagerUtils.h"


#include "nsString.h"

#include "nsRunAppRun.h"

#include <X11/Xutil.h>

NS_IMPL_QUERY_HEAD(nsWindow)
NS_IMPL_QUERY_BODY(nsIWindow)
NS_IMPL_QUERY_BODY(nsPIWindowXlib)
NS_IMPL_QUERY_BODY(nsISupportsWeakReference)
NS_IMPL_QUERY_TAIL_INHERITING(nsDrawable)

NS_IMPL_ADDREF_INHERITED(nsWindow, nsDrawable)
NS_IMPL_RELEASE_INHERITED(nsWindow, nsDrawable)


nsWindow::nsWindow() :
  mWindow(0),
  mParent(nsnull),
  mMapped(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsWindow::~nsWindow()
{
}

/* nsIWindow methods */
/* attribute nsIGUIEventListener eventListener; */
NS_IMETHODIMP nsWindow::GetEventListener(nsIGUIEventListener * *aEventListener)
{
  *aEventListener = mEventListener;
  NS_IF_ADDREF(*aEventListener);

  return NS_OK;
}
NS_IMETHODIMP nsWindow::SetEventListener(nsIGUIEventListener * aEventListener)
{
  mEventListener = aEventListener;

  return NS_OK;
}

/* attribute nsIWindow parent; */
NS_IMETHODIMP nsWindow::GetParent(nsIWindow * *aParent)
{
  nsCOMPtr<nsIWindow> parent = do_QueryReferent(mParent);
  if (parent) {
    *aParent = parent;
    NS_ADDREF(*aParent);
  } else {
    *aParent = nsnull;
  }

  return NS_OK;
}
NS_IMETHODIMP nsWindow::SetParent(nsIWindow * aParent)
{
  /* what if we get a null parent .. is this right? */

  Window newParent = 0;

  if (aParent) {
    mParent = getter_AddRefs(NS_GetWeakReference(aParent));

    newParent = NS_STATIC_CAST(nsWindow*, aParent)->mWindow;
  }

  ::XReparentWindow(mDisplay, mWindow, newParent, 0, 0);

  return NS_OK;
}

/* readonly attribute boolean isVisible; */
NS_IMETHODIMP nsWindow::GetIsVisible(PRBool *aVisibility)
{
  *aVisibility = mMapped;
  return NS_OK;
}

/* void show ( ); */
NS_IMETHODIMP nsWindow::Show()
{
  ::XRaiseWindow(mDisplay, mWindow);
  ::XMapWindow(mDisplay, mWindow);

  mMapped = PR_TRUE;

  return NS_OK;
}

/* void show ( ); */
NS_IMETHODIMP nsWindow::Hide()
{
  ::XUnmapWindow(mDisplay, mWindow);

  mMapped = PR_FALSE;

  return NS_OK;
}

/* void move (in gfx_coord aX, in gfx_coord aY); */
NS_IMETHODIMP nsWindow::Move(gfx_coord aX, gfx_coord aY)
{
  /* should we set these here or wait until the ConfigureNotify comes in? */
  mBounds.MoveTo(aX, aY);

  ::XMoveWindow(mDisplay, mWindow, aX, aY);

  return NS_OK;
}

/* void resize (in gfx_dimension aWidth, in gfx_dimension aHeight, in boolean aRepaint); */
NS_IMETHODIMP nsWindow::Resize(gfx_dimension aWidth, gfx_dimension aHeight, PRBool aRepaint)
{
  /* should we set these here or wait until the ConfigureNotify comes in? */
  mBounds.SizeTo(aWidth, aHeight);

  ::XResizeWindow(mDisplay, mWindow, aWidth, aHeight);

  return NS_OK;
}

/* void moveResize (in gfx_coord aX, in gfx_coord aY, in gfx_dimension aWidth, in gfx_dimension aHeight, in boolean aRepaint); */
NS_IMETHODIMP nsWindow::MoveResize(gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight, PRBool aRepaint)
{
  /* should we set these here or wait until the ConfigureNotify comes in? */
  mBounds.SetRect(aX, aY, aWidth, aHeight);

  ::XMoveResizeWindow(mDisplay, mWindow, aX, aY, aWidth, aHeight);

  return NS_OK;
}

/* void getBounds (out gfx_coord aX, out gfx_coord aY, out gfx_dimension aWidth, out gfx_dimension aHeight); */
NS_IMETHODIMP nsWindow::GetBounds(gfx_coord *aX, gfx_coord *aY, gfx_dimension *aWidth, gfx_dimension *aHeight)
{
  *aX = mBounds.x;
  *aY = mBounds.y;
  *aWidth = mBounds.width;
  *aHeight = mBounds.height;

  return NS_OK;
}

/* attribute gfx_color winBackgroundColor; */
NS_IMETHODIMP nsWindow::GetWinBackgroundColor(gfx_color *aBackgroundColor)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsWindow::SetWinBackgroundColor(gfx_color aBackgroundColor)
{
  // do some kind of color transformation?
  ::XSetWindowBackground(mDisplay, mWindow, aBackgroundColor);
  return NS_OK;
}

/* attribute nsICursor cursor; */
NS_IMETHODIMP nsWindow::GetCursor(nsICursor * *aCursor)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsWindow::SetCursor(nsICursor * aCursor)
{
  if (!aCursor)
    return NS_ERROR_FAILURE;

  Cursor cursor = NS_STATIC_CAST(nsCursor*, aCursor)->mCursor;

  ::XDefineCursor(mDisplay, mWindow, cursor);

  return NS_OK;
}

NS_IMETHODIMP nsWindow::WidgetToScreen(const nsRect2 & aOldRect, nsRect2 & aNewRect)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsWindow::CaptureRollupEvents(nsIRollupListener *aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void invalidateRect ([const] in nsRect2 aRect, in boolean aIsSynchronous); */
NS_IMETHODIMP nsWindow::InvalidateRect(const nsRect2 * aRect, PRBool aIsSynchronous)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void invalidateRegion (in nsIRegion aRegion, in boolean aIsSynchronous); */
NS_IMETHODIMP nsWindow::InvalidateRegion(nsIRegion *aRegion, PRBool aIsSynchronous)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void update (); */
NS_IMETHODIMP nsWindow::Update()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void scroll (in gfx_coord aSrcX, in gfx_coord aSrcY, in gfx_coord aDestX, in gfx_coord aDestY, in gfx_dimension aWidth, in gfx_dimension aHeight); */
NS_IMETHODIMP nsWindow::Scroll(gfx_coord aSrcX, gfx_coord aSrcY, gfx_coord aDestX, gfx_coord aDestY, gfx_dimension aWidth, gfx_dimension aHeight)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}



/* nsPIWindowXlib methods */

/* void initFromNative (in Window window); */
NS_IMETHODIMP nsWindow::InitFromNative(Window window)
{
  if (!window)
    return NS_ERROR_FAILURE;

  mWindow = window;
  Window rootWindow;
  int x, y;
  unsigned int width, height;
  unsigned int border_width;
  ::XGetGeometry(mDisplay, window, &rootWindow, &x, &y, &width, &height, &border_width,
                 &NS_STATIC_CAST(unsigned int, mDepth));

  mBounds.SetRect(x, y, width, height);

  // only if its inputoutput, then we should SetDrawable()
  SetDrawable((Drawable)mWindow);

  return NS_OK;
}

/* readonly attribute Window nativeWindow; */
NS_IMETHODIMP nsWindow::GetNativeWindow(Window *aNativeWindow)
{
  *aNativeWindow = mWindow;

  return NS_OK;
}

/* void dispatchEvent (in nsGUIEvent aEvent); */
NS_IMETHODIMP nsWindow::DispatchEvent(nsGUIEvent * aEvent)
{
  if (!mEventListener)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIWindow> kungFuDeathGrip(this);
  mEventListener->ProcessEvent(aEvent);

  return NS_OK;
}
