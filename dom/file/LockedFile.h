/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_lockedfile_h__
#define mozilla_dom_file_lockedfile_h__

#include "FileCommon.h"

#include "nsIDOMLockedFile.h"
#include "nsIRunnable.h"

#include "nsDOMEventTargetHelper.h"

class nsIInputStream;

BEGIN_FILE_NAMESPACE

class FileHandle;
class FileRequest;
class MetadataHelper;

class LockedFile : public nsDOMEventTargetHelper,
                   public nsIDOMLockedFile,
                   public nsIRunnable
{
  friend class FinishHelper;
  friend class FileService;
  friend class FileHelper;
  friend class MetadataHelper;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMLOCKEDFILE
  NS_DECL_NSIRUNNABLE

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(LockedFile, nsDOMEventTargetHelper)

  enum Mode
  {
    READ_ONLY = 0,
    READ_WRITE
  };

  enum RequestMode
  {
    NORMAL = 0, // Sequential
    PARALLEL
  };

  enum ReadyState
  {
    INITIAL = 0,
    LOADING,
    FINISHING,
    DONE
  };

  static already_AddRefed<LockedFile>
  Create(FileHandle* aFileHandle,
         Mode aMode,
         RequestMode aRequestMode = NORMAL);

  // nsIDOMEventTarget
  virtual nsresult
  PreHandleEvent(nsEventChainPreVisitor& aVisitor);

  nsresult
  CreateParallelStream(nsISupports** aStream);

  nsresult
  GetOrCreateStream(nsISupports** aStream);

  bool
  IsOpen() const;

  bool
  IsAborted() const
  {
    return mAborted;
  }

  FileHandle*
  Handle() const
  {
    return mFileHandle;
  }

  nsresult
  OpenInputStream(bool aWholeFile, PRUint64 aStart, PRUint64 aLength,
                  nsIInputStream** aResult);

private:
  LockedFile();
  ~LockedFile();

  void
  OnNewRequest();

  void
  OnRequestFinished();

  inline already_AddRefed<FileRequest>
  GenerateFileRequest();

  nsresult
  WriteOrAppend(const jsval& aValue, JSContext* aCx,
                nsIDOMFileRequest** _retval, bool aAppend);

  nsresult
  Finish();

  nsRefPtr<FileHandle> mFileHandle;
  ReadyState mReadyState;
  Mode mMode;
  RequestMode mRequestMode;
  PRUint64 mLocation;
  PRUint32 mPendingRequests;

  NS_DECL_EVENT_HANDLER(complete)
  NS_DECL_EVENT_HANDLER(abort)
  NS_DECL_EVENT_HANDLER(error)

  nsTArray<nsCOMPtr<nsISupports> > mParallelStreams;
  nsCOMPtr<nsISupports> mStream;

  bool mAborted;
  bool mCreating;
};

class FinishHelper MOZ_FINAL : public nsIRunnable
{
  friend class LockedFile;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

private:
  FinishHelper(LockedFile* aLockedFile);
  ~FinishHelper()
  { }

  nsRefPtr<LockedFile> mLockedFile;
  nsTArray<nsCOMPtr<nsISupports> > mParallelStreams;
  nsCOMPtr<nsISupports> mStream;

  bool mAborted;
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_lockedfile_h__
