/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMFileHandle.h"

#include "nsIFileStreams.h"

#include "nsDOMClassInfoID.h"
#include "nsNetUtil.h"

#include "File.h"
#include "LockedFile.h"

USING_FILE_NAMESPACE

// static
already_AddRefed<DOMFileHandle>
DOMFileHandle::Create(nsPIDOMWindow* aWindow,
                      nsIFileStorage* aFileStorage,
                      nsIFile* aFile)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<DOMFileHandle> newFile(new DOMFileHandle);

  newFile->BindToOwner(aWindow);

  newFile->mFileStorage = aFileStorage;
  nsresult rv = aFile->GetLeafName(newFile->mName);
  NS_ENSURE_SUCCESS(rv, nullptr);

  newFile->mFile = aFile;
  newFile->mFileName = newFile->mName;

  return newFile.forget();
}

already_AddRefed<nsISupports>
DOMFileHandle::CreateStream(nsIFile* aFile, bool aReadOnly)
{
  nsresult rv;

  if (aReadOnly) {
    nsCOMPtr<nsIInputStream> stream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), aFile, -1, -1,
                                    nsIFileInputStream::DEFER_OPEN);
    NS_ENSURE_SUCCESS(rv, nullptr);
    return stream.forget();
  }

  nsCOMPtr<nsIFileStream> stream;
  rv = NS_NewLocalFileStream(getter_AddRefs(stream), aFile, -1, -1,
                             nsIFileStream::DEFER_OPEN);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return stream.forget();
}

already_AddRefed<nsIDOMFile>
DOMFileHandle::CreateFileObject(LockedFile* aLockedFile, uint32_t aFileSize)
{
  nsCOMPtr<nsIDOMFile> file = 
    new File(mName, mType, aFileSize, mFile, aLockedFile);

  return file.forget();
}
