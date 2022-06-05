/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRPermissionRequest_h_
#define mozilla_dom_XRPermissionRequest_h_

#include "mozilla/dom/Promise.h"
#include "nsContentPermissionHelper.h"
#include "nsISupports.h"

namespace mozilla::dom {

/**
 * Handles permission dialog management when requesting XR device access.
 */
class XRPermissionRequest final : public ContentPermissionRequestBase {
 public:
  XRPermissionRequest(nsPIDOMWindowInner* aWindow, uint64_t aWindowId);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XRPermissionRequest,
                                           ContentPermissionRequestBase)
  // nsIContentPermissionRequest
  NS_IMETHOD Cancel(void) override;
  NS_IMETHOD Allow(JS::Handle<JS::Value> choices) override;
  nsresult Start();

 private:
  ~XRPermissionRequest() = default;

  uint64_t mWindowId;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_XR_h_
