/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "fs/FileSystemThreadSafeStreamOwner.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/dom/FileSystemLog.h"
#include "mozilla/dom/FileSystemWritableFileStream.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsIOutputStream.h"
#include "nsIRandomAccessStream.h"

namespace mozilla::dom::fs {

namespace {

nsresult TruncFile(nsCOMPtr<nsIRandomAccessStream>& aStream, int64_t aEOF) {
  int64_t offset = 0;
  QM_TRY(MOZ_TO_RESULT(aStream->Tell(&offset)));

  QM_TRY(MOZ_TO_RESULT(aStream->Seek(nsISeekableStream::NS_SEEK_SET, aEOF)));

  QM_TRY(MOZ_TO_RESULT(aStream->SetEOF()));

  QM_TRY(MOZ_TO_RESULT(aStream->Seek(nsISeekableStream::NS_SEEK_END, 0)));

  // Restore original offset
  QM_TRY(MOZ_TO_RESULT(aStream->Seek(nsISeekableStream::NS_SEEK_SET, offset)));

  return NS_OK;
}

}  // namespace

FileSystemThreadSafeStreamOwner::FileSystemThreadSafeStreamOwner(
    FileSystemWritableFileStream* aWritableFileStream,
    nsCOMPtr<nsIRandomAccessStream>&& aStream)
    :
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
      mWritableFileStream(aWritableFileStream),
#endif
      mStream(std::forward<nsCOMPtr<nsIRandomAccessStream>>(aStream)),
      mClosed(false) {
  MOZ_ASSERT(mWritableFileStream);
}

nsresult FileSystemThreadSafeStreamOwner::Truncate(uint64_t aSize) {
  MOZ_DIAGNOSTIC_ASSERT(mWritableFileStream->IsCommandActive());

  if (mClosed) {  // Multiple closes can end up in a queue
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  int64_t where = 0;
  QM_TRY(MOZ_TO_RESULT(mStream->Tell(&where)));

  // Truncate filehandle (can extend with 0's)
  LOG(("%p: Truncate to %" PRIu64, this, aSize));
  QM_TRY(MOZ_TO_RESULT(TruncFile(mStream, aSize)));

  // Per non-normative text in the spec (2.5.3) we should adjust
  // the cursor position to be within the new file size
  if (static_cast<uint64_t>(where) > aSize) {
    QM_TRY(MOZ_TO_RESULT(mStream->Seek(nsISeekableStream::NS_SEEK_END, 0)));
  }

  return NS_OK;
}

nsresult FileSystemThreadSafeStreamOwner::Seek(uint64_t aPosition) {
  MOZ_DIAGNOSTIC_ASSERT(mWritableFileStream->IsCommandActive());

  if (mClosed) {  // Multiple closes can end up in a queue
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  const auto checkedPosition = CheckedInt<int64_t>(aPosition);
  if (!checkedPosition.isValid()) {
    return NS_ERROR_INVALID_ARG;
  }

  return mStream->Seek(nsISeekableStream::NS_SEEK_SET, checkedPosition.value());
}

void FileSystemThreadSafeStreamOwner::Close() {
  if (mClosed) {  // Multiple closes can end up in a queue
    return;
  }

  mClosed = true;
  mStream->OutputStream()->Close();
}

nsCOMPtr<nsIOutputStream> FileSystemThreadSafeStreamOwner::OutputStream() {
  MOZ_DIAGNOSTIC_ASSERT(mWritableFileStream->IsCommandActive());

  return mStream->OutputStream();
}

}  // namespace mozilla::dom::fs
