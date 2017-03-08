/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GetFileOrDirectory_h
#define mozilla_dom_GetFileOrDirectory_h

#include "mozilla/dom/Directory.h"
#include "mozilla/dom/FileSystemTaskBase.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {

class BlobImpl;
class FileSystemGetFileOrDirectoryParams;

class GetFileOrDirectoryTaskChild final : public FileSystemTaskChildBase
{
public:
  static already_AddRefed<GetFileOrDirectoryTaskChild>
  Create(FileSystemBase* aFileSystem,
         nsIFile* aTargetPath,
         ErrorResult& aRv);

  virtual
  ~GetFileOrDirectoryTaskChild();

  already_AddRefed<Promise>
  GetPromise();

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
  GetFileOrDirectoryTaskChild(FileSystemBase* aFileSystem,
                              nsIFile* aTargetPath);

  RefPtr<Promise> mPromise;
  nsCOMPtr<nsIFile> mTargetPath;

  RefPtr<File> mResultFile;
  RefPtr<Directory> mResultDirectory;
};

class GetFileOrDirectoryTaskParent final : public FileSystemTaskParentBase
{
public:
  static already_AddRefed<GetFileOrDirectoryTaskParent>
  Create(FileSystemBase* aFileSystem,
         const FileSystemGetFileOrDirectoryParams& aParam,
         FileSystemRequestParent* aParent,
         ErrorResult& aRv);

protected:
  virtual FileSystemResponseValue
  GetSuccessRequestResult(ErrorResult& aRv) const override;

  virtual nsresult
  IOWork() override;

private:
  GetFileOrDirectoryTaskParent(FileSystemBase* aFileSystem,
                               const FileSystemGetFileOrDirectoryParams& aParam,
                               FileSystemRequestParent* aParent);

  nsCOMPtr<nsIFile> mTargetPath;

  // Whether we get a directory.
  bool mIsDirectory;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_GetFileOrDirectory_h
