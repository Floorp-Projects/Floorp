/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRViewport.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(XRViewport, mParent)

XRViewport::XRViewport(nsISupports* aParent, const gfx::IntRect& aRect)
    : mParent(aParent), mRect(aRect) {}

JSObject* XRViewport::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return XRViewport_Binding::Wrap(aCx, this, aGivenProto);
}

int32_t XRViewport::X() { return mRect.X(); }

int32_t XRViewport::Y() { return mRect.Y(); }

int32_t XRViewport::Width() { return mRect.Width(); }

int32_t XRViewport::Height() { return mRect.Height(); }

}  // namespace mozilla::dom
