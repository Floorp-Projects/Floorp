/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StructuredCloneHelper_h
#define mozilla_dom_StructuredCloneHelper_h

#include "js/StructuredClone.h"
#include "nsAutoPtr.h"
#include "nsISupports.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class StructuredCloneHelperInternal
{
public:
  StructuredCloneHelperInternal();
  virtual ~StructuredCloneHelperInternal();

  // These methods should be implemented in order to clone data.
  // Read more documentation in js/public/StructuredClone.h.

  virtual JSObject* ReadCallback(JSContext* aCx,
                                 JSStructuredCloneReader* aReader,
                                 uint32_t aTag,
                                 uint32_t aIndex) = 0;

  virtual bool WriteCallback(JSContext* aCx,
                             JSStructuredCloneWriter* aWriter,
                             JS::Handle<JSObject*> aObj) = 0;

  // This method has to be called when this object is not needed anymore.
  // It will free memory and the buffer. This has to be called because
  // otherwise the buffer will be freed in the DTOR of this class and at that
  // point we cannot use the overridden methods.
  void Shutdown();

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

#ifdef DEBUG
  bool mShutdownCalled;
#endif
};

class MessagePortBase;
class MessagePortIdentifier;

class StructuredCloneHelper : public StructuredCloneHelperInternal
{
public:
  enum StructuredCloneHelperFlags {
    eAll = 0,

    // Disable the cloning of blobs. If a blob is part of the cloning value,
    // an exception will be thrown.
    eBlobNotSupported = 1 << 0,

    // Disable the cloning of FileLists. If a FileList is part of the cloning
    // value, an exception will be thrown.
    eFileListNotSupported = 1 << 1,

    // MessagePort can just be transfered. Using this flag we do not support
    // the transfering.
    eMessagePortNotSupported = 1 << 2,
  };

  // aFlags is a bitmap of StructuredCloneHelperFlags.
  explicit StructuredCloneHelper(uint32_t aFlags = eAll);
  virtual ~StructuredCloneHelper();

  bool Write(JSContext* aCx,
             JS::Handle<JS::Value> aValue,
             JS::Handle<JS::Value> aTransfer);

  bool Read(nsISupports* aParent,
            JSContext* aCx,
            JS::MutableHandle<JS::Value> aValue);

  nsTArray<nsRefPtr<MessagePortBase>>& GetTransferredPorts()
  {
    MOZ_ASSERT(!(mFlags & eMessagePortNotSupported));
    return mTransferredPorts;
  }

  // Custom Callbacks

  virtual JSObject* ReadCallback(JSContext* aCx,
                                 JSStructuredCloneReader* aReader,
                                 uint32_t aTag,
                                 uint32_t aIndex) override;

  virtual bool WriteCallback(JSContext* aCx,
                             JSStructuredCloneWriter* aWriter,
                             JS::Handle<JSObject*> aObj) override;

  virtual bool ReadTransferCallback(JSContext* aCx,
                                    JSStructuredCloneReader* aReader,
                                    uint32_t aTag,
                                    void* aContent,
                                    uint64_t aExtraData,
                                    JS::MutableHandleObject aReturnObject) override;

  virtual bool WriteTransferCallback(JSContext* aCx,
                                     JS::Handle<JSObject*> aObj,
                                     uint32_t* aTag,
                                     JS::TransferableOwnership* aOwnership,
                                     void** aContent,
                                     uint64_t* aExtraData) override;

  virtual void FreeTransferCallback(uint32_t aTag,
                                    JS::TransferableOwnership aOwnership,
                                    void* aContent,
                                    uint64_t aExtraData) override;
private:
  bool StoreISupports(nsISupports* aSupports)
  {
    MOZ_ASSERT(aSupports);
    mSupportsArray.AppendElement(aSupports);
    return true;
  }

  // This is our bitmap.
  uint32_t mFlags;

  // Useful for the structured clone algorithm:

  nsTArray<nsCOMPtr<nsISupports>> mSupportsArray;

  // This raw pointer is set and unset into the ::Read(). It's always null
  // outside that method. For this reason it's a raw pointer.
  nsISupports* MOZ_NON_OWNING_REF mParent;

  // This hashtable contains the ports while doing write (transferring and
  // mapping transferred objects to the objects in the clone). It's an empty
  // array outside the 'Write()' method.
  nsTArray<nsRefPtr<MessagePortBase>> mTransferringPort;

  // This array contains the ports once we've finished the reading. It's
  // generated from the mPortIdentifiers array.
  nsTArray<nsRefPtr<MessagePortBase>> mTransferredPorts;

  // This array contains the identifiers of the MessagePorts. Based on these we
  // are able to reconnect the new transferred ports with the other
  // MessageChannel ports.
  nsTArray<MessagePortIdentifier> mPortIdentifiers;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_StructuredCloneHelper_h
