/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ArchiveEvent.h"

#include "nsCExternalHandlerService.h"
#include "nsProxyRelease.h"

USING_ARCHIVEREADER_NAMESPACE

NS_IMPL_ISUPPORTS0(ArchiveItem)

ArchiveItem::ArchiveItem()
{
  MOZ_COUNT_CTOR(ArchiveItem);
}

ArchiveItem::~ArchiveItem()
{
  MOZ_COUNT_DTOR(ArchiveItem);
}


nsCString
ArchiveItem::GetType()
{
  if (mType.IsEmpty()) {
    return NS_LITERAL_CSTRING("binary/octet-stream");
  }

  return mType;
}

void
ArchiveItem::SetType(const nsCString& aType)
{
  mType = aType;
}

ArchiveReaderEvent::ArchiveReaderEvent(ArchiveReader* aArchiveReader)
: mArchiveReader(aArchiveReader)
{
  MOZ_COUNT_CTOR(ArchiveReaderEvent);
}

ArchiveReaderEvent::~ArchiveReaderEvent()
{
  if (!NS_IsMainThread()) {
    nsIMIMEService* mimeService;
    mMimeService.forget(&mimeService);

    if (mimeService) {
      nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
      NS_WARN_IF_FALSE(mainThread, "Couldn't get the main thread! Leaking!");

      if (mainThread) {
        NS_ProxyRelease(mainThread, mimeService);
      }
    }
  }

  MOZ_COUNT_DTOR(ArchiveReaderEvent);
}

// From the filename to the mimetype:
nsresult
ArchiveReaderEvent::GetType(nsCString& aExt,
                            nsCString& aMimeType)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  
  if (mMimeService.get() == nullptr) {
    mMimeService = do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = mMimeService->GetTypeFromExtension(aExt, aMimeType);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
ArchiveReaderEvent::Run()
{
  return Exec();
}

nsresult
ArchiveReaderEvent::RunShare(nsresult aStatus)
{
  mStatus = aStatus;

  nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &ArchiveReaderEvent::ShareMainThread);
  NS_DispatchToMainThread(event);

  return NS_OK;
}

void
ArchiveReaderEvent::ShareMainThread()
{
  nsTArray<nsCOMPtr<nsIDOMFile> > fileList;

  if (!NS_FAILED(mStatus)) {
    // This extra step must run in the main thread:
    for (uint32_t index = 0; index < mFileList.Length(); ++index) {
      nsRefPtr<ArchiveItem> item = mFileList[index];

      nsString tmp;
      nsresult rv = item->GetFilename(tmp);
      nsCString filename = NS_ConvertUTF16toUTF8(tmp);
      if (NS_FAILED(rv)) {
        continue;
      }

      int32_t offset = filename.RFindChar('.');
      if (offset != kNotFound) {
        filename.Cut(0, offset + 1);

        // Just to be sure, if something goes wrong, the mimetype is an empty string:
        nsCString type;
        if (NS_SUCCEEDED(GetType(filename, type))) {
          item->SetType(type);
        }
      }

      // This is a File:
      nsRefPtr<nsIDOMFile> file = item->File(mArchiveReader);
      if (file) {
        fileList.AppendElement(file);
      }
    }
  }

  mArchiveReader->Ready(fileList, mStatus);
}
