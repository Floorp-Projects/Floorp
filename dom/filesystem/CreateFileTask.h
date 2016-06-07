/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CreateFileTask_h
#define mozilla_dom_CreateFileTask_h

#include "mozilla/dom/FileSystemTaskBase.h"
#include "mozilla/ErrorResult.h"

class nsIInputStream;

namespace mozilla {
namespace dom {

class Blob;
class BlobImpl;
class Promise;

class CreateFileTaskChild final : public FileSystemTaskChildBase
{
public:
  static already_AddRefed<CreateFileTaskChild>
  Create(FileSystemBase* aFileSystem,
         nsIFile* aFile,
         Blob* aBlobData,
         InfallibleTArray<uint8_t>& aArrayData,
         bool replace,
         ErrorResult& aRv);

  virtual
  ~CreateFileTaskChild();

  already_AddRefed<Promise>
  GetPromise();

  virtual void
  GetPermissionAccessType(nsCString& aAccess) const override;

protected:
  virtual FileSystemParams
  GetRequestParams(const nsString& aSerializedDOMPath,
                   ErrorResult& aRv) const override;

  virtual void
  SetSuccessRequestResult(const FileSystemResponseValue& aValue,
                          ErrorResult& aRv) override;

  virtual void
  HandlerCallback() override;

private:
  CreateFileTaskChild(FileSystemBase* aFileSystem,
                      nsIFile* aFile,
                      bool aReplace);

  RefPtr<Promise> mPromise;
  nsCOMPtr<nsIFile> mTargetPath;

  RefPtr<BlobImpl> mBlobImpl;

  // This is going to be the content of the file, received by createFile()
  // params.
  InfallibleTArray<uint8_t> mArrayData;

  bool mReplace;
};

class CreateFileTaskParent final : public FileSystemTaskParentBase
{
public:
  static already_AddRefed<CreateFileTaskParent>
  Create(FileSystemBase* aFileSystem,
         const FileSystemCreateFileParams& aParam,
         FileSystemRequestParent* aParent,
         ErrorResult& aRv);

  virtual bool
  NeedToGoToMainThread() const override { return true; }

  virtual nsresult
  MainThreadWork() override;

  virtual void
  GetPermissionAccessType(nsCString& aAccess) const override;

protected:
  virtual FileSystemResponseValue
  GetSuccessRequestResult(ErrorResult& aRv) const override;

  virtual nsresult
  IOWork() override;

private:
  CreateFileTaskParent(FileSystemBase* aFileSystem,
                       const FileSystemCreateFileParams& aParam,
                       FileSystemRequestParent* aParent);

  static uint32_t sOutputBufferSize;

  nsCOMPtr<nsIFile> mTargetPath;

  RefPtr<BlobImpl> mBlobImpl;

  // This is going to be the content of the file, received by createFile()
  // params.
  InfallibleTArray<uint8_t> mArrayData;

  bool mReplace;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CreateFileTask_h
