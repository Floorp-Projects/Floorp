/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_filehandle_h__
#define mozilla_dom_file_filehandle_h__

#include "FileCommon.h"

#include "nsIDOMFileHandle.h"
#include "nsIFile.h"

#include "nsDOMEventTargetHelper.h"

class nsIDOMFile;
class nsIFileStorage;

BEGIN_FILE_NAMESPACE

class FileService;
class LockedFile;
class FinishHelper;
class FileHelper;

/**
 * Must be subclassed. The subclass must implement CreateStream and
 * CreateFileObject. Basically, every file storage implementation provides its
 * own FileHandle implementation (for example IDBFileHandle provides IndexedDB
 * specific implementation).
 */
class FileHandle : public nsDOMEventTargetHelper,
                   public nsIDOMFileHandle
{
  friend class FileService;
  friend class LockedFile;
  friend class FinishHelper;
  friend class FileHelper;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMFILEHANDLE

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileHandle, nsDOMEventTargetHelper)

  const nsAString&
  Name() const
  {
    return mName;
  }

  const nsAString&
  Type() const
  {
    return mType;
  }

  virtual already_AddRefed<nsISupports>
  CreateStream(nsIFile* aFile, bool aReadOnly) = 0;

  virtual already_AddRefed<nsIDOMFile>
  CreateFileObject(LockedFile* aLockedFile, PRUint32 aFileSize) = 0;

protected:
  FileHandle()
  { }

  ~FileHandle()
  { }

  nsCOMPtr<nsIFileStorage> mFileStorage;

  nsString mName;
  nsString mType;

  nsCOMPtr<nsIFile> mFile;
  nsString mFileName;

  NS_DECL_EVENT_HANDLER(abort)
  NS_DECL_EVENT_HANDLER(error)
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_filehandle_h__
