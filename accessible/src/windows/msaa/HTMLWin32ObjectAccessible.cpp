/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLWin32ObjectAccessible.h"

#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HTMLWin32ObjectOwnerAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLWin32ObjectOwnerAccessible::
  HTMLWin32ObjectOwnerAccessible(nsIContent* aContent,
                                 DocAccessible* aDoc, void* aHwnd) :
  AccessibleWrap(aContent, aDoc), mHwnd(aHwnd)
{
  // Our only child is a HTMLWin32ObjectAccessible object.
  if (mHwnd)
    mNativeAccessible = new HTMLWin32ObjectAccessible(mHwnd);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLWin32ObjectOwnerAccessible: Accessible implementation

void
HTMLWin32ObjectOwnerAccessible::Shutdown()
{
  AccessibleWrap::Shutdown();
  mNativeAccessible = nullptr;
}

role
HTMLWin32ObjectOwnerAccessible::NativeRole()
{
  return roles::EMBEDDED_OBJECT;
}

bool
HTMLWin32ObjectOwnerAccessible::NativelyUnavailable() const
{
  // XXX: No HWND means this is windowless plugin which is not accessible in
  // the meantime.
  return !mHwnd;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLWin32ObjectOwnerAccessible: Accessible protected implementation

void
HTMLWin32ObjectOwnerAccessible::CacheChildren()
{
  if (mNativeAccessible)
    AppendChild(mNativeAccessible);
}


////////////////////////////////////////////////////////////////////////////////
// HTMLWin32ObjectAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLWin32ObjectAccessible::HTMLWin32ObjectAccessible(void* aHwnd) :
  DummyAccessible()
{
  mHwnd = aHwnd;
  if (mHwnd) {
    // The plugin is not windowless. In this situation we use 
    // use its inner child owned by the plugin so that we don't get
    // in an infinite loop, where the WM_GETOBJECT's get forwarded
    // back to us and create another HTMLWin32ObjectAccessible
    HWND childWnd = ::GetWindow((HWND)aHwnd, GW_CHILD);
    if (childWnd) {
      mHwnd = childWnd;
    }
  }
}

NS_IMETHODIMP 
HTMLWin32ObjectAccessible::GetNativeInterface(void** aNativeAccessible)
{
  if (mHwnd) {
    ::AccessibleObjectFromWindow(static_cast<HWND>(mHwnd),
                                 OBJID_WINDOW, IID_IAccessible,
                                 aNativeAccessible);
  }
  return NS_OK;
}

