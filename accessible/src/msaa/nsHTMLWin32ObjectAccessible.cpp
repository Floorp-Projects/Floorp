/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLWin32ObjectAccessible.h"

#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsHTMLWin32ObjectOwnerAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLWin32ObjectOwnerAccessible::
  nsHTMLWin32ObjectOwnerAccessible(nsIContent* aContent,
                                   DocAccessible* aDoc, void* aHwnd) :
  nsAccessibleWrap(aContent, aDoc), mHwnd(aHwnd)
{
  // Our only child is a nsHTMLWin32ObjectAccessible object.
  mNativeAccessible = new nsHTMLWin32ObjectAccessible(mHwnd);
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLWin32ObjectOwnerAccessible: nsAccessNode implementation

void
nsHTMLWin32ObjectOwnerAccessible::Shutdown()
{
  nsAccessibleWrap::Shutdown();
  mNativeAccessible = nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLWin32ObjectOwnerAccessible: nsAccessible implementation

role
nsHTMLWin32ObjectOwnerAccessible::NativeRole()
{
  return roles::EMBEDDED_OBJECT;
}

PRUint64
nsHTMLWin32ObjectOwnerAccessible::NativeState()
{
  // XXX: No HWND means this is windowless plugin which is not accessible in
  // the meantime.
  return mHwnd ? nsAccessibleWrap::NativeState() : states::UNAVAILABLE;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLWin32ObjectOwnerAccessible: nsAccessible protected implementation

void
nsHTMLWin32ObjectOwnerAccessible::CacheChildren()
{
  if (mNativeAccessible)
    AppendChild(mNativeAccessible);
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLWin32ObjectAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLWin32ObjectAccessible::nsHTMLWin32ObjectAccessible(void* aHwnd):
nsLeafAccessible(nsnull, nsnull)
{
  // XXX: Mark it as defunct to make sure no single nsAccessible method is
  // running on it. We need to allow accessible without DOM nodes.
  mFlags |= eIsDefunct;

  mHwnd = aHwnd;
  if (mHwnd) {
    // The plugin is not windowless. In this situation we use 
    // use its inner child owned by the plugin so that we don't get
    // in an infinite loop, where the WM_GETOBJECT's get forwarded
    // back to us and create another nsHTMLWin32ObjectAccessible
    HWND childWnd = ::GetWindow((HWND)aHwnd, GW_CHILD);
    if (childWnd) {
      mHwnd = childWnd;
    }
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLWin32ObjectAccessible, nsAccessible)

NS_IMETHODIMP 
nsHTMLWin32ObjectAccessible::GetNativeInterface(void** aNativeAccessible)
{
  if (mHwnd) {
    ::AccessibleObjectFromWindow(static_cast<HWND>(mHwnd),
                                 OBJID_WINDOW, IID_IAccessible,
                                 aNativeAccessible);
  }
  return NS_OK;
}

