/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CreateFileTask_h
#define mozilla_dom_CreateFileTask_h

#include "mozilla/dom/FileSystemTaskBase.h"
#include "nsAutoPtr.h"
#include "mozilla/ErrorResult.h"

class nsIInputStream;

namespace mozilla {
namespace dom {

class File;
class FileImpl;
class Promise;

class CreateFileTask final
  : public FileSystemTaskBase
{
public:
  CreateFileTask(FileSystemBase* aFileSystem,
                 const nsAString& aPath,
                 File* aBlobData,
                 InfallibleTArray<uint8_t>& aArrayData,
                 bool replace,
                 ErrorResult& aRv);
  CreateFileTask(FileSystemBase* aFileSystem,
                 const FileSystemCreateFileParams& aParam,
                 FileSystemRequestParent* aParent);

  virtual
  ~CreateFileTask();

  already_AddRefed<Promise>
  GetPromise();

  virtual void
  GetPermissionAccessType(nsCString& aAccess) const override;

protected:
  virtual FileSystemParams
  GetRequestParams(const nsString& aFileSystem) const override;

  virtual FileSystemResponseValue
  GetSuccessRequestResult() const override;

  virtual void
  SetSuccessRequestResult(const FileSystemResponseValue& aValue) override;

  virtual nsresult
  Work() override;

  virtual void
  HandlerCallback() override;

private:
  void
  GetOutputBufferSize() const;

  static uint32_t sOutputBufferSize;
  nsRefPtr<Promise> mPromise;
  nsString mTargetRealPath;

  // Not thread-safe and should be released on main thread.
  nsRefPtr<File> mBlobData;

  nsCOMPtr<nsIInputStream> mBlobStream;
  InfallibleTArray<uint8_t> mArrayData;
  bool mReplace;

  // This cannot be a File because this object is created on a different
  // thread and File is not thread-safe. Let's use the FileImpl instead.
  nsRefPtr<FileImpl> mTargetFileImpl;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CreateFileTask_h
