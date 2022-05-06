/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TemporaryIPCBlobChild_h
#define mozilla_dom_TemporaryIPCBlobChild_h

#include "mozilla/dom/PTemporaryIPCBlob.h"
#include "mozilla/dom/PTemporaryIPCBlobChild.h"

namespace mozilla::dom {

class BlobImpl;
class MutableBlobStorage;

class TemporaryIPCBlobChildCallback {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void OperationSucceeded(BlobImpl* aBlobImpl) = 0;
  virtual void OperationFailed(nsresult aRv) = 0;
};

class TemporaryIPCBlobChild final : public PTemporaryIPCBlobChild {
  friend class PTemporaryIPCBlobChild;

 public:
  NS_INLINE_DECL_REFCOUNTING(TemporaryIPCBlobChild)

  explicit TemporaryIPCBlobChild(MutableBlobStorage* aStorage);

  void AskForBlob(TemporaryIPCBlobChildCallback* aCallback,
                  const nsACString& aContentType, PRFileDesc* aFD);

 private:
  ~TemporaryIPCBlobChild() override;

  mozilla::ipc::IPCResult RecvFileDesc(const FileDescriptor& aFD);

  mozilla::ipc::IPCResult Recv__delete__(const IPCBlobOrError& aBlobOrError);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  RefPtr<MutableBlobStorage> mMutableBlobStorage;
  RefPtr<TemporaryIPCBlobChildCallback> mCallback;
  bool mActive;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_TemporaryIPCBlobChild_h
