/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WEBRTC_GLOBAL_PARENT_H_
#define _WEBRTC_GLOBAL_PARENT_H_

#include "mozilla/dom/PWebrtcGlobalParent.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {

class WebrtcParents;

class WebrtcGlobalParent : public PWebrtcGlobalParent {
  friend class ContentParent;
  friend class WebrtcGlobalInformation;
  friend class WebrtcContentParents;

  bool mShutdown;

  MOZ_IMPLICIT WebrtcGlobalParent();

  static WebrtcGlobalParent* Alloc();
  static bool Dealloc(WebrtcGlobalParent* aActor);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual mozilla::ipc::IPCResult Recv__delete__() override;

  virtual ~WebrtcGlobalParent();

 public:
  NS_INLINE_DECL_REFCOUNTING(WebrtcGlobalParent)

  bool IsActive() { return !mShutdown; }
};

}  // namespace dom
}  // namespace mozilla

#endif  // _WEBRTC_GLOBAL_PARENT_H_
