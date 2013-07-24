/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_domarchiveevent_h__
#define mozilla_dom_file_domarchiveevent_h__

#include "ArchiveReader.h"

#include "nsISeekableStream.h"
#include "nsIMIMEService.h"
#include "nsDOMFile.h"

#include "FileCommon.h"

BEGIN_FILE_NAMESPACE

/**
 * This class contains all the info needed for a single item
 * It must contain the implementation of the File() method.
 */
class ArchiveItem : public nsISupports
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  ArchiveItem();
  virtual ~ArchiveItem();

  // Getter/Setter for the type
  nsCString GetType();
  void SetType(const nsCString& aType);

  // Getter for the filename
  virtual nsresult GetFilename(nsString& aFilename) = 0;

  // Generate a DOMFile
  virtual nsIDOMFile* File(ArchiveReader* aArchiveReader) = 0;

protected:
  nsCString mType;
};

/**
 * This class must be extended by any archive format supported by ArchiveReader API
 * This class runs in a different thread and it calls the 'exec()' method.
 * The exec() must populate mFileList and mStatus then it must call RunShare();
 */
class ArchiveReaderEvent : public nsRunnable
{
public:
  NS_DECL_NSIRUNNABLE

  ArchiveReaderEvent(ArchiveReader* aArchiveReader);

  virtual ~ArchiveReaderEvent();

  // This must be implemented
  virtual nsresult Exec() = 0;

protected:
  nsresult GetType(nsCString& aExt,
                   nsCString& aMimeType);

  nsresult RunShare(nsresult aStatus);
  void ShareMainThread();

protected: // data
  ArchiveReader* mArchiveReader;

  nsCOMPtr<nsIMIMEService> mMimeService;

  nsTArray<nsRefPtr<ArchiveItem> > mFileList; // this must be populated
  nsresult mStatus;
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_domarchiveevent_h__

