/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ArchiveEvent.h"

#include "nsContentUtils.h"
#include "nsCExternalHandlerService.h"

USING_FILE_NAMESPACE

NS_IMPL_THREADSAFE_ISUPPORTS0(ArchiveItem)

ArchiveItem::~ArchiveItem()
{
}


nsCString
ArchiveItem::GetType()
{
  return mType.IsEmpty() ? nsCString("binary/octet-stream") : mType;
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
  MOZ_COUNT_DTOR(ArchiveReaderEvent);
}

// From the filename to the mimetype:
nsresult
ArchiveReaderEvent::GetType(nsCString& aExt,
                            nsCString& aMimeType)
{
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
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);

  return NS_OK;
}

void
ArchiveReaderEvent::ShareMainThread()
{
  nsTArray<nsCOMPtr<nsIDOMFile> > fileList;

  if (!NS_FAILED(mStatus)) {
    // This extra step must run in the main thread:
    for (PRUint32 index = 0; index < mFileList.Length(); ++index) {
      nsRefPtr<ArchiveItem> item = mFileList[index];

      PRInt32 offset = item->GetFilename().RFindChar('.');
      if (offset != kNotFound) {
        nsCString ext(item->GetFilename());
        ext.Cut(0, offset + 1);

        // Just to be sure, if something goes wrong, the mimetype is an empty string:
        nsCString type;
        if (GetType(ext, type) == NS_OK)
          item->SetType(type);
      }

      // This is a nsDOMFile:
      nsRefPtr<nsIDOMFile> file = item->File(mArchiveReader);
      fileList.AppendElement(file);
    }
  }

  mArchiveReader->Ready(fileList, mStatus);
}
