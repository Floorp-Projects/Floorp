/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TemporaryFileBlobImpl.h"

#include "IPCBlobInputStreamThread.h"
#include "nsFileStreams.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

namespace {

// Here the flags needed in order to keep the temporary file opened.
// 1. REOPEN_ON_REWIND -> otherwise the stream is not serializable more than
//                        once.
// 2. no DEFER_OPEN -> the file must be kept open on windows in order to be
//                     deleted when used.
// 3. no CLOSE_ON_EOF -> the file will be closed by the DTOR. No needs. Also
//                       because the inputStream will not be read directly.
// 4. no SHARE_DELETE -> We don't want to allow this file to be deleted.
const uint32_t sTemporaryFileStreamFlags =
  nsIFileInputStream::REOPEN_ON_REWIND;

class TemporaryFileInputStream final : public nsFileInputStream
{
public:
  static nsresult
  Create(nsIFile* aFile, nsIInputStream** aInputStream)
  {
    MOZ_ASSERT(aFile);
    MOZ_ASSERT(aInputStream);
    MOZ_ASSERT(XRE_IsParentProcess());

    RefPtr<TemporaryFileInputStream> stream =
      new TemporaryFileInputStream(aFile);

    nsresult rv = stream->Init(aFile, -1, -1, sTemporaryFileStreamFlags);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    stream.forget(aInputStream);
    return NS_OK;
  }

  void
  Serialize(InputStreamParams& aParams,
            FileDescriptorArray& aFileDescriptors) override
  {
    MOZ_CRASH("This inputStream cannot be serialized.");
  }

  bool
  Deserialize(const InputStreamParams& aParams,
              const FileDescriptorArray& aFileDescriptors) override
  {
    MOZ_CRASH("This inputStream cannot be deserialized.");
    return false;
  }

private:
  explicit TemporaryFileInputStream(nsIFile* aFile)
    : mFile(aFile)
  {
    MOZ_ASSERT(XRE_IsParentProcess());
  }

  ~TemporaryFileInputStream()
  {
    // Let's delete the file on the IPCBlob Thread.
    RefPtr<IPCBlobInputStreamThread> thread =
      IPCBlobInputStreamThread::GetOrCreate();
    if (NS_WARN_IF(!thread)) {
      return;
    }

    nsCOMPtr<nsIFile> file = std::move(mFile);
    thread->Dispatch(NS_NewRunnableFunction(
      "TemporaryFileInputStream::Runnable",
      [file]() {
        file->Remove(false);
      }
    ));
  }

  nsCOMPtr<nsIFile> mFile;
};

} // anonymous

TemporaryFileBlobImpl::TemporaryFileBlobImpl(nsIFile* aFile,
                                             const nsAString& aContentType)
  : FileBlobImpl(aFile, EmptyString(), aContentType)
#ifdef DEBUG
  , mInputStreamCreated(false)
#endif
{
  MOZ_ASSERT(XRE_IsParentProcess());
}

TemporaryFileBlobImpl::~TemporaryFileBlobImpl()
{
  MOZ_ASSERT(mInputStreamCreated);
}

already_AddRefed<BlobImpl>
TemporaryFileBlobImpl::CreateSlice(uint64_t aStart, uint64_t aLength,
                                   const nsAString& aContentType,
                                   ErrorResult& aRv)
{
  MOZ_CRASH("This BlobImpl is not meant to be sliced!");
  return nullptr;
}

void
TemporaryFileBlobImpl::CreateInputStream(nsIInputStream** aStream, ErrorResult& aRv)
{
#ifdef DEBUG
  MOZ_ASSERT(!mInputStreamCreated);
  // CreateInputStream can be called only once.
  mInputStreamCreated = true;
#endif

  aRv = TemporaryFileInputStream::Create(mFile, aStream);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

} // namespace dom
} // namespace mozilla
