/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessagePortUtils_h
#define mozilla_dom_MessagePortUtils_h

#include "MessagePort.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/PMessagePort.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {
namespace messageport {

struct
StructuredCloneClosure
{
  nsTArray<nsRefPtr<BlobImpl>> mBlobImpls;
  nsTArray<MessagePortIdentifier> mMessagePortIdentifiers;
};

struct
StructuredCloneData
{
  StructuredCloneData() : mData(nullptr), mDataLength(0) {}
  uint64_t* mData;
  size_t mDataLength;
  StructuredCloneClosure mClosure;
};

bool
ReadStructuredCloneWithTransfer(JSContext* aCx, nsTArray<uint8_t>& aData,
                                const StructuredCloneClosure& aClosure,
                                JS::MutableHandle<JS::Value> aClone,
                                nsPIDOMWindow* aParentWindow,
                                nsTArray<nsRefPtr<MessagePort>>& aMessagePorts);

bool
WriteStructuredCloneWithTransfer(JSContext* aCx, JS::Handle<JS::Value> aSource,
                                 JS::Handle<JS::Value> aTransferable,
                                 nsTArray<uint8_t>& aData,
                                 StructuredCloneClosure& aClosure);

void
FreeStructuredClone(nsTArray<uint8_t>& aData,
                    StructuredCloneClosure& aClosure);

} // namespace messageport
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MessagePortUtils_h
