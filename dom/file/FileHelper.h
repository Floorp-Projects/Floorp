/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_filehelper_h__
#define mozilla_dom_file_filehelper_h__

#include "FileCommon.h"

#include "nsIRequestObserver.h"
#include "nsThreadUtils.h"

class nsIFileStorage;

BEGIN_FILE_NAMESPACE

class FileHelper;
class FileRequest;
class FileOutputStreamWrapper;
class LockedFile;

class FileHelperListener
{
public:
  NS_IMETHOD_(nsrefcnt)
  AddRef() = 0;

  NS_IMETHOD_(nsrefcnt)
  Release() = 0;

  virtual void
  OnFileHelperComplete(FileHelper* aFileHelper) = 0;
};

/**
 * Must be subclassed. The subclass must implement DoAsyncRun. It may then
 * choose to implement GetSuccessResult to properly set the result of the
 * success event. Call Enqueue to start the file operation.
 */
class FileHelper : public nsIRequestObserver
{
  friend class FileRequest;
  friend class FileOutputStreamWrapper;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER

  nsresult
  Enqueue();

  nsresult
  AsyncRun(FileHelperListener* aListener);

  void
  OnStreamProgress(uint64_t aProgress, uint64_t aProgressMax);

  void
  OnStreamClose()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    Finish();
  }

  void
  OnStreamDestroy()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    Finish();
  }

  static LockedFile*
  GetCurrentLockedFile();

protected:
  FileHelper(LockedFile* aLockedFile, FileRequest* aRequest);

  virtual ~FileHelper();

  virtual nsresult
  DoAsyncRun(nsISupports* aStream) = 0;

  virtual nsresult
  GetSuccessResult(JSContext* aCx, JS::Value* aVal);

  virtual void
  ReleaseObjects();

  void
  Finish();

  nsCOMPtr<nsIFileStorage> mFileStorage;
  nsRefPtr<LockedFile> mLockedFile;
  nsRefPtr<FileRequest> mFileRequest;

  nsRefPtr<FileHelperListener> mListener;
  nsCOMPtr<nsIRequest> mRequest;

private:
  nsresult mResultCode;
  bool mFinished;
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_filehelper_h__
