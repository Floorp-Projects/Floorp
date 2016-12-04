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
  mStateFlags |= eNoKidsFromDOM;

  // Our only child is a HTMLWin32ObjectAccessible object.
  if (mHwnd) {
    mNativeAccessible = new HTMLWin32ObjectAccessible(mHwnd, aDoc);
    AppendChild(mNativeAccessible);
  }
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
// HTMLWin32ObjectAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLWin32ObjectAccessible::HTMLWin32ObjectAccessible(void* aHwnd,
                                                     DocAccessible* aDoc) :
  DummyAccessible(aDoc)
{
  mHwnd = aHwnd;
  if (mHwnd) {
#if defined(MOZ_CONTENT_SANDBOX)
    if (XRE_IsContentProcess()) {
      DocAccessibleChild* ipcDoc = aDoc->IPCDoc();
      MOZ_ASSERT(ipcDoc);
      if (!ipcDoc) {
        return;
      }

      IAccessibleHolder proxyHolder;
      if (!ipcDoc->SendGetWindowedPluginIAccessible(
              reinterpret_cast<uintptr_t>(mHwnd), &proxyHolder)) {
        return;
      }

      mCOMProxy.reset(proxyHolder.Release());
      return;
    }
#endif

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

void
HTMLWin32ObjectAccessible::GetNativeInterface(void** aNativeAccessible)
{
#if defined(MOZ_CONTENT_SANDBOX)
  if (XRE_IsContentProcess()) {
    RefPtr<IAccessible> addRefed = mCOMProxy.get();
    addRefed.forget(aNativeAccessible);
    return;
  }
#endif

  if (mHwnd) {
    ::AccessibleObjectFromWindow(static_cast<HWND>(mHwnd),
                                 OBJID_WINDOW, IID_IAccessible,
                                 aNativeAccessible);
  }
}

