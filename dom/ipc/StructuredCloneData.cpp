/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StructuredCloneData.h"

#include "nsIDOMDOMException.h"
#include "nsIMutable.h"
#include "nsIXPConnect.h"

#include "ipc/IPCMessageUtils.h"
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
namespace ipc {

bool
StructuredCloneData::Copy(const StructuredCloneData& aData)
{
  if (!aData.mInitialized) {
    return true;
  }

  if (aData.SharedData()) {
    mSharedData = aData.SharedData();
  } else {
    mSharedData =
      SharedJSAllocatedData::CreateFromExternalData(aData.Data());
    NS_ENSURE_TRUE(mSharedData, false);
  }

  PortIdentifiers().AppendElements(aData.PortIdentifiers());

  MOZ_ASSERT(BlobImpls().IsEmpty());
  BlobImpls().AppendElements(aData.BlobImpls());

  MOZ_ASSERT(GetSurfaces().IsEmpty());

  mInitialized = true;

  return true;
}

void
StructuredCloneData::Read(JSContext* aCx,
                          JS::MutableHandle<JS::Value> aValue,
                          ErrorResult &aRv)
{
  MOZ_ASSERT(mInitialized);

  nsIGlobalObject *global = xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));
  MOZ_ASSERT(global);

  ReadFromBuffer(global, aCx, Data(), aValue, aRv);
}

void
StructuredCloneData::Write(JSContext* aCx,
                           JS::Handle<JS::Value> aValue,
                           ErrorResult &aRv)
{
  Write(aCx, aValue, JS::UndefinedHandleValue, aRv);
}

void
StructuredCloneData::Write(JSContext* aCx,
                           JS::Handle<JS::Value> aValue,
                           JS::Handle<JS::Value> aTransfer,
                           ErrorResult &aRv)
{
  MOZ_ASSERT(!mInitialized);

  StructuredCloneHolder::Write(aCx, aValue, aTransfer, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  JSStructuredCloneData data;
  mBuffer->abandon();
  mBuffer->steal(&data);
  mBuffer = nullptr;
  mSharedData = new SharedJSAllocatedData(Move(data));
  mInitialized = true;
}

void
StructuredCloneData::WriteIPCParams(IPC::Message* aMsg) const
{
  WriteParam(aMsg, Data());
}

bool
StructuredCloneData::ReadIPCParams(const IPC::Message* aMsg,
                                   PickleIterator* aIter)
{
  MOZ_ASSERT(!mInitialized);
  JSStructuredCloneData data;
  if (!ReadParam(aMsg, aIter, &data)) {
    return false;
  }
  mSharedData = new SharedJSAllocatedData(Move(data));
  mInitialized = true;
  return true;
}

bool
StructuredCloneData::CopyExternalData(const char* aData,
                                      size_t aDataLength)
{
  MOZ_ASSERT(!mInitialized);
  mSharedData = SharedJSAllocatedData::CreateFromExternalData(aData,
                                                              aDataLength);
  NS_ENSURE_TRUE(mSharedData, false);
  mInitialized = true;
  return true;
}

} // namespace ipc
} // namespace dom
} // namespace mozilla
