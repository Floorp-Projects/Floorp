/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StructuredCloneHelper.h"

namespace mozilla {
namespace dom {

namespace {

JSObject*
StructuredCloneCallbacksRead(JSContext* aCx,
                             JSStructuredCloneReader* aReader,
                             uint32_t aTag, uint32_t aIndex,
                             void* aClosure)
{
  StructuredCloneHelperInternal* helper =
    static_cast<StructuredCloneHelperInternal*>(aClosure);
  MOZ_ASSERT(helper);
  return helper->ReadCallback(aCx, aReader, aTag, aIndex);
}

bool
StructuredCloneCallbacksWrite(JSContext* aCx,
                              JSStructuredCloneWriter* aWriter,
                              JS::Handle<JSObject*> aObj,
                              void* aClosure)
{
  StructuredCloneHelperInternal* helper =
    static_cast<StructuredCloneHelperInternal*>(aClosure);
  MOZ_ASSERT(helper);
  return helper->WriteCallback(aCx, aWriter, aObj);
}

bool
StructuredCloneCallbacksReadTransfer(JSContext* aCx,
                                     JSStructuredCloneReader* aReader,
                                     uint32_t aTag,
                                     void* aContent,
                                     uint64_t aExtraData,
                                     void* aClosure,
                                     JS::MutableHandleObject aReturnObject)
{
  StructuredCloneHelperInternal* helper =
    static_cast<StructuredCloneHelperInternal*>(aClosure);
  MOZ_ASSERT(helper);
  return helper->ReadTransferCallback(aCx, aReader, aTag, aContent,
                                      aExtraData, aReturnObject);
}

bool
StructuredCloneCallbacksWriteTransfer(JSContext* aCx,
                                      JS::Handle<JSObject*> aObj,
                                      void* aClosure,
                                      // Output:
                                      uint32_t* aTag,
                                      JS::TransferableOwnership* aOwnership,
                                      void** aContent,
                                      uint64_t* aExtraData)
{
  StructuredCloneHelperInternal* helper =
    static_cast<StructuredCloneHelperInternal*>(aClosure);
  MOZ_ASSERT(helper);
  return helper->WriteTransferCallback(aCx, aObj, aTag, aOwnership, aContent,
                                       aExtraData);
}

void
StructuredCloneCallbacksFreeTransfer(uint32_t aTag,
                                     JS::TransferableOwnership aOwnership,
                                     void* aContent,
                                     uint64_t aExtraData,
                                     void* aClosure)
{
  StructuredCloneHelperInternal* helper =
    static_cast<StructuredCloneHelperInternal*>(aClosure);
  MOZ_ASSERT(helper);
  return helper->FreeTransferCallback(aTag, aOwnership, aContent, aExtraData);
}

void
StructuredCloneCallbacksError(JSContext* aCx,
                              uint32_t aErrorId)
{
  NS_WARNING("Failed to clone data for the Console API in workers.");
}

const JSStructuredCloneCallbacks gCallbacks = {
  StructuredCloneCallbacksRead,
  StructuredCloneCallbacksWrite,
  StructuredCloneCallbacksError,
  StructuredCloneCallbacksReadTransfer,
  StructuredCloneCallbacksWriteTransfer,
  StructuredCloneCallbacksFreeTransfer
};

} // anonymous namespace

bool
StructuredCloneHelperInternal::Write(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue)
{
  MOZ_ASSERT(!mBuffer, "Double Write is not allowed");

  mBuffer = new JSAutoStructuredCloneBuffer(&gCallbacks, this);
  return mBuffer->write(aCx, aValue, &gCallbacks, this);
}

bool
StructuredCloneHelperInternal::Read(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aValue)
{
  MOZ_ASSERT(mBuffer, "Read() without Write() is not allowed.");

  bool ok = mBuffer->read(aCx, aValue, &gCallbacks, this);
  mBuffer = nullptr;
  return ok;
}

bool
StructuredCloneHelperInternal::ReadTransferCallback(JSContext* aCx,
                                                    JSStructuredCloneReader* aReader,
                                                    uint32_t aTag,
                                                    void* aContent,
                                                    uint64_t aExtraData,
                                                    JS::MutableHandleObject aReturnObject)
{
  MOZ_CRASH("Nothing to read.");
  return false;
}


bool
StructuredCloneHelperInternal::WriteTransferCallback(JSContext* aCx,
                                                     JS::Handle<JSObject*> aObj,
                                                     uint32_t* aTag,
                                                     JS::TransferableOwnership* aOwnership,
                                                     void** aContent,
                                                     uint64_t* aExtraData)
{
  // No transfers are supported by default.
  return false;
}

void
StructuredCloneHelperInternal::FreeTransferCallback(uint32_t aTag,
                                                    JS::TransferableOwnership aOwnership,
                                                    void* aContent,
                                                    uint64_t aExtraData)
{
  MOZ_CRASH("Nothing to free.");
}

} // dom namespace
} // mozilla namespace
