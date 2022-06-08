/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerCloneData.h"

#include <utility>
#include "mozilla/RefPtr.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "nsISerialEventTarget.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

namespace mozilla::dom {

ServiceWorkerCloneData::~ServiceWorkerCloneData() {
  RefPtr<ipc::SharedJSAllocatedData> sharedData = TakeSharedData();
  if (sharedData) {
    NS_ProxyRelease(__func__, mEventTarget, sharedData.forget());
  }
}

ServiceWorkerCloneData::ServiceWorkerCloneData()
    : ipc::StructuredCloneData(
          StructuredCloneHolder::StructuredCloneScope::UnknownDestination,
          StructuredCloneHolder::TransferringSupported),
      mEventTarget(GetCurrentSerialEventTarget()),
      mIsErrorMessageData(false) {
  MOZ_DIAGNOSTIC_ASSERT(mEventTarget);
}

bool ServiceWorkerCloneData::BuildClonedMessageData(
    ClonedOrErrorMessageData& aClonedData) {
  if (IsErrorMessageData()) {
    aClonedData = ErrorMessageData();
    return true;
  }

  MOZ_DIAGNOSTIC_ASSERT(
      CloneScope() ==
      StructuredCloneHolder::StructuredCloneScope::DifferentProcess);

  ClonedMessageData messageData;
  if (!StructuredCloneData::BuildClonedMessageData(messageData)) {
    return false;
  }

  aClonedData = std::move(messageData);

  return true;
}

void ServiceWorkerCloneData::CopyFromClonedMessageData(
    const ClonedOrErrorMessageData& aClonedData) {
  if (aClonedData.type() == ClonedOrErrorMessageData::TErrorMessageData) {
    mIsErrorMessageData = true;
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(aClonedData.type() ==
                        ClonedOrErrorMessageData::TClonedMessageData);

  StructuredCloneData::CopyFromClonedMessageData(aClonedData);
}

void ServiceWorkerCloneData::SetAsErrorMessageData() {
  MOZ_ASSERT(CloneScope() ==
             StructuredCloneHolder::StructuredCloneScope::SameProcess);

  mIsErrorMessageData = true;
}

bool ServiceWorkerCloneData::IsErrorMessageData() const {
  return mIsErrorMessageData;
}

}  // namespace mozilla::dom
