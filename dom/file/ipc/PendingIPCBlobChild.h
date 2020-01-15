/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PendingIPCBlobChild_h
#define mozilla_dom_PendingIPCBlobChild_h

#include "mozilla/dom/PPendingIPCBlob.h"
#include "mozilla/dom/PPendingIPCBlobChild.h"

namespace mozilla {
namespace dom {

class BlobImpl;

class PendingIPCBlobChild final : public PPendingIPCBlobChild {
 public:
  explicit PendingIPCBlobChild(const IPCBlob& aBlob);

  // After calling one of the following method, the actor will be deleted.

  // For File.
  already_AddRefed<BlobImpl> SetPendingInfoAndDeleteActor(
      const nsString& aName, const nsString& aContentType, uint64_t aLength,
      int64_t aLastModifiedDate);

  // For Blob.
  already_AddRefed<BlobImpl> SetPendingInfoAndDeleteActor(
      const nsString& aContentType, uint64_t aLength);

 private:
  ~PendingIPCBlobChild();

  RefPtr<BlobImpl> mBlobImpl;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PendingIPCBlobChild_h
