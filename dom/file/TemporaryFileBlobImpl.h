/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TemporaryFileBlobImpl_h
#define mozilla_dom_TemporaryFileBlobImpl_h

#include "FileBlobImpl.h"

namespace mozilla {
namespace dom {

// This class is meant to be used by TemporaryIPCBlobParent only.
// Don't use it for anything else, please!
// Note that CreateInputStream() _must_ be called, and called just once!

// This class is a BlobImpl because it needs to be sent via IPC using
// IPCBlobUtils.
class TemporaryFileBlobImpl final : public FileBlobImpl
{
#ifdef DEBUG
  bool mInputStreamCreated;
#endif

public:
  explicit TemporaryFileBlobImpl(nsIFile* aFile, const nsAString& aContentType);

  // Overrides
  virtual void CreateInputStream(nsIInputStream** aInputStream,
                                 ErrorResult& aRv) override;

protected:
  virtual ~TemporaryFileBlobImpl();

private:
  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType, ErrorResult& aRv) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TemporaryFileBlobImpl_h
