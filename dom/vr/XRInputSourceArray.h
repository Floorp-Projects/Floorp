/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRInputSourceArray_h_
#define mozilla_dom_XRInputSourceArray_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WebXRBinding.h"

#include "gfxVR.h"

namespace mozilla {
namespace gfx {
struct VRControllerState;
}
namespace dom {
class XRInputSource;

class XRInputSourceArray final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(XRInputSourceArray)

  explicit XRInputSourceArray(nsISupports* aParent);

  // WebIDL Boilerplate
  nsISupports* GetParentObject() const { return mParent; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Members
  XRInputSource* IndexedGetter(uint32_t aIndex, bool& aFound);
  uint32_t Length();
  void Setup(XRSession* aSession, RefPtr<gfx::VRDisplayClient> aDisplayClient);
  void Update(XRSession* aSession);
  void Clear(XRSession* aSession);

 protected:
  virtual ~XRInputSourceArray() = default;

 private:
  void DispatchInputSourceRemovedEvent(const nsTArray<RefPtr<XRInputSource>>& aInputs,
                                       XRSession* aSession);

  nsCOMPtr<nsISupports> mParent;
  nsTArray<RefPtr<XRInputSource>> mInputSources;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRInputSourceArray_h_
