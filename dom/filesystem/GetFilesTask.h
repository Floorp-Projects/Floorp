/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GetFilesTask_h
#define mozilla_dom_GetFilesTask_h

#include "mozilla/dom/Directory.h"
#include "mozilla/dom/FileSystemTaskBase.h"
#include "mozilla/ErrorResult.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace dom {

class BlobImpl;

class GetFilesTaskChild final : public FileSystemTaskChildBase
{
public:
  static already_AddRefed<GetFilesTaskChild>
  Create(FileSystemBase* aFileSystem,
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
  nsCOMPtr<nsIFile> mTargetPath;
  bool mRecursiveFlag;

  // We store the fullpath of Files.
  FallibleTArray<nsString> mTargetData;
};

class GetFilesTaskParent final : public FileSystemTaskParentBase
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

  nsresult
  ExploreDirectory(nsIFile* aPath);

  nsCOMPtr<nsIFile> mTargetPath;
  bool mRecursiveFlag;

  // We store the fullpath of Files.
  FallibleTArray<nsString> mTargetData;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_GetFilesTask_h
