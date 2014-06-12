/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileHelper_h
#define mozilla_dom_FileHelper_h

#include "js/TypeDecls.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIRequestObserver.h"

namespace mozilla {
namespace dom {

class FileHandle;
class FileHelper;
class FileRequest;
class FileOutputStreamWrapper;
class MutableFile;

class FileHelperListener
{
public:
  NS_IMETHOD_(MozExternalRefCountType)
  AddRef() = 0;

  NS_IMETHOD_(MozExternalRefCountType)
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
  OnStreamClose();

  void
  OnStreamDestroy();

  static FileHandle*
  GetCurrentFileHandle();

protected:
  FileHelper(FileHandle* aFileHandle, FileRequest* aRequest);

  virtual ~FileHelper();

  virtual nsresult
  DoAsyncRun(nsISupports* aStream) = 0;

  virtual nsresult
  GetSuccessResult(JSContext* aCx, JS::MutableHandle<JS::Value> aVal);

  virtual void
  ReleaseObjects();

  void
  Finish();

  nsRefPtr<MutableFile> mMutableFile;
  nsRefPtr<FileHandle> mFileHandle;
  nsRefPtr<FileRequest> mFileRequest;

  nsRefPtr<FileHelperListener> mListener;
  nsCOMPtr<nsIRequest> mRequest;

private:
  nsresult mResultCode;
  bool mFinished;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileHelper_h
