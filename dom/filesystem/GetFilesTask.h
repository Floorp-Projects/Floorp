/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GetFilesTask_h
#define mozilla_dom_GetFilesTask_h

#include "mozilla/dom/Directory.h"
#include "mozilla/dom/FileSystemTaskBase.h"
#include "mozilla/dom/GetFilesHelper.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {

class BlobImpl;

class GetFilesTaskChild final : public FileSystemTaskChildBase
{
public:
  static already_AddRefed<GetFilesTaskChild>
  Create(FileSystemBase* aFileSystem,
         Directory* aDirectory,
         nsIFile* aTargetPath,
         bool aRecursiveFlag,
         ErrorResult& aRv);

  virtual
  ~GetFilesTaskChild();

  already_AddRefed<Promise>
  GetPromise();

  virtual void
  GetPermissionAccessType(nsCString& aAccess) const override;

private:
  // If aDirectoryOnly is set, we should ensure that the target is a directory.
  GetFilesTaskChild(FileSystemBase* aFileSystem,
                    Directory* aDirectory,
                    nsIFile* aTargetPath,
                    bool aRecursiveFlag);

  virtual FileSystemParams
  GetRequestParams(const nsString& aSerializedDOMPath,
                   ErrorResult& aRv) const override;

  virtual void
  SetSuccessRequestResult(const FileSystemResponseValue& aValue,
                          ErrorResult& aRv) override;

  virtual void
  HandlerCallback() override;

  RefPtr<Promise> mPromise;
  RefPtr<Directory> mDirectory;
  nsCOMPtr<nsIFile> mTargetPath;
  bool mRecursiveFlag;

  // We store the fullpath and the dom path of Files.
  struct FileData {
    nsString mRealPath;
    nsString mDOMPath;
  };

  FallibleTArray<FileData> mTargetData;
};

class GetFilesTaskParent final : public FileSystemTaskParentBase
                               , public GetFilesHelperBase
{
public:
  static already_AddRefed<GetFilesTaskParent>
  Create(FileSystemBase* aFileSystem,
         const FileSystemGetFilesParams& aParam,
         FileSystemRequestParent* aParent,
         ErrorResult& aRv);

  virtual void
  GetPermissionAccessType(nsCString& aAccess) const override;

private:
  GetFilesTaskParent(FileSystemBase* aFileSystem,
                     const FileSystemGetFilesParams& aParam,
                     FileSystemRequestParent* aParent);

  virtual FileSystemResponseValue
  GetSuccessRequestResult(ErrorResult& aRv) const override;

  virtual nsresult
  IOWork() override;

  nsString mDirectoryDOMPath;
  nsCOMPtr<nsIFile> mTargetPath;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_GetFilesTask_h
