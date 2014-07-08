/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PCOMContentPermissionRequestChild_h
#define PCOMContentPermissionRequestChild_h

#include "mozilla/dom/PContentPermissionRequestChild.h"
// Microsoft's API Name hackery sucks
// XXXbz Doing this in a header is a gigantic footgun.  See
// https://bugzilla.mozilla.org/show_bug.cgi?id=932421#c3 for why.
#undef CreateEvent

/*
  PContentPermissionRequestChild implementations also are
  XPCOM objects.  Addref() is called on their implementation
  before SendPContentPermissionRequestConstructor is called.
  When Dealloc is called, IPDLRelease() is called.
  Implementations of this method are expected to call
  Release() on themselves.  See Bug 594261 for more
  information.
 */
class PCOMContentPermissionRequestChild : public mozilla::dom::PContentPermissionRequestChild {
public:
  virtual void IPDLRelease() = 0;
#ifdef DEBUG
  PCOMContentPermissionRequestChild() : mIPCOpen(false) {}
  virtual ~PCOMContentPermissionRequestChild() {
    // mIPCOpen is set to true in TabChild::SendPContentPermissionRequestConstructor
    // and set to false in TabChild::DeallocPContentPermissionRequest
    MOZ_ASSERT(!mIPCOpen, "Protocol must not be open when PCOMContentPermissionRequestChild is destroyed.");
  }
  bool mIPCOpen;
#endif /* DEBUG */
};

#endif
