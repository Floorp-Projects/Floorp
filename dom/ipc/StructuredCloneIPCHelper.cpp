/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StructuredCloneIPCHelper.h"

#include "nsIDOMDOMException.h"
#include "nsIMutable.h"
#include "nsIXPConnect.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsContentUtils.h"
#include "nsJSEnvironment.h"
#include "MainThreadUtils.h"
#include "StructuredCloneTags.h"
#include "jsapi.h"

namespace mozilla {
namespace dom {

bool
StructuredCloneIPCHelper::Copy(const StructuredCloneIPCHelper& aHelper)
{
  if (!aHelper.mData) {
    return true;
  }

  uint64_t* data = static_cast<uint64_t*>(malloc(aHelper.mDataLength));
  if (!data) {
    return false;
  }

  memcpy(data, aHelper.mData, aHelper.mDataLength);

  mData = data;
  mDataLength = aHelper.mDataLength;
  mDataOwned = eAllocated;

  MOZ_ASSERT(BlobImpls().IsEmpty());
  BlobImpls().AppendElements(aHelper.BlobImpls());

  MOZ_ASSERT(GetImages().IsEmpty());

  return true;
}

void
StructuredCloneIPCHelper::Read(JSContext* aCx,
                               JS::MutableHandle<JS::Value> aValue,
                               ErrorResult &aRv)
{
  MOZ_ASSERT(mData);

  nsIGlobalObject *global = xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));
  MOZ_ASSERT(global);

  ReadFromBuffer(global, aCx, mData, mDataLength, aValue, aRv);
}

void
StructuredCloneIPCHelper::Write(JSContext* aCx,
                                JS::Handle<JS::Value> aValue,
                                ErrorResult &aRv)
{
  MOZ_ASSERT(!mData);

  StructuredCloneHelper::Write(aCx, aValue, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  mBuffer->steal(&mData, &mDataLength);
  mBuffer = nullptr;
  mDataOwned = eJSAllocated;
}

} // namespace dom
} // namespace mozilla
