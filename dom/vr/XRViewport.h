/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRViewport_h_
#define mozilla_dom_XRViewport_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WebXRBinding.h"
#include "mozilla/gfx/Rect.h"

#include "gfxVR.h"

namespace mozilla::dom {

class XRViewport final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(XRViewport)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(XRViewport)

  explicit XRViewport(nsISupports* aParent, const gfx::IntRect& aRect);

  // WebIDL Boilerplate
  nsISupports* GetParentObject() const { return mParent; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Members
  int32_t X();
  int32_t Y();
  int32_t Width();
  int32_t Height();

 protected:
  virtual ~XRViewport() = default;

  nsCOMPtr<nsISupports> mParent;

 public:
  gfx::IntRect mRect;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_XRViewport_h_
