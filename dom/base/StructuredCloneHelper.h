/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StructuredCloneHelper_h
#define mozilla_dom_StructuredCloneHelper_h

#include "js/StructuredClone.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace dom {

class StructuredCloneHelperInternal
{
public:
  // These methods should be implemented in order to clone data.
  // Read more documentation in js/public/StructuredClone.h.

  virtual JSObject* ReadCallback(JSContext* aCx,
                                 JSStructuredCloneReader* aReader,
                                 uint32_t aTag,
                                 uint32_t aIndex) = 0;

  virtual bool WriteCallback(JSContext* aCx,
                             JSStructuredCloneWriter* aWriter,
                             JS::Handle<JSObject*> aObj) = 0;

  // If these 3 methods are not implement, transfering objects will not be
  // allowed.

  virtual bool
  ReadTransferCallback(JSContext* aCx,
                       JSStructuredCloneReader* aReader,
                       uint32_t aTag,
                       void* aContent,
                       uint64_t aExtraData,
                       JS::MutableHandleObject aReturnObject);

  virtual bool
  WriteTransferCallback(JSContext* aCx,
                        JS::Handle<JSObject*> aObj,
                        // Output:
                        uint32_t* aTag,
                        JS::TransferableOwnership* aOwnership,
                        void** aContent,
                        uint64_t* aExtraData);

  virtual void
  FreeTransferCallback(uint32_t aTag,
                       JS::TransferableOwnership aOwnership,
                       void* aContent,
                       uint64_t aExtraData);

  // These methods are what you should use.

  bool Write(JSContext* aCx,
             JS::Handle<JS::Value> aValue);

  bool Write(JSContext* aCx,
             JS::Handle<JS::Value> aValue,
             JS::Handle<JS::Value> aTransfer);

  bool Read(JSContext* aCx,
            JS::MutableHandle<JS::Value> aValue);

protected:
  nsAutoPtr<JSAutoStructuredCloneBuffer> mBuffer;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_StructuredCloneHelper_h
