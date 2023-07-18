/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageAccessPermissionStatus_h_
#define mozilla_dom_StorageAccessPermissionStatus_h_

#include "mozilla/dom/PermissionStatus.h"

namespace mozilla::dom {

class StorageAccessPermissionStatus final : public PermissionStatus {
 public:
  static RefPtr<CreatePromise> Create(nsPIDOMWindowInner* aWindow);

 private:
  explicit StorageAccessPermissionStatus(nsPIDOMWindowInner* aWindow);

  RefPtr<SimplePromise> UpdateState() override;

  bool MaybeUpdatedBy(nsIPermission* aPermission) const override;
  bool MaybeUpdatedByNotifyOnly(
      nsPIDOMWindowInner* aInnerWindow) const override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_StorageAccessPermissionStatus_h_
