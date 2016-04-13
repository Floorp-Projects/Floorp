/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoveTask_h
#define mozilla_dom_RemoveTask_h

#include "mozilla/dom/FileSystemTaskBase.h"
#include "nsAutoPtr.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {

class BlobImpl;
class Promise;

class RemoveTaskChild final : public FileSystemTaskChildBase
{
public:
  static already_AddRefed<RemoveTaskChild>
  Create(FileSystemBase* aFileSystem,
         nsIFile* aDirPath,
         nsIFile* aTargetPath,
         bool aRecursive,
         ErrorResult& aRv);

  virtual
  ~RemoveTaskChild();

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
  RemoveTaskChild(FileSystemBase* aFileSystem,
                  nsIFile* aDirPath,
                  nsIFile* aTargetPath,
                  bool aRecursive);

  RefPtr<Promise> mPromise;

  // This path is the Directory::mFile.
  nsCOMPtr<nsIFile> mDirPath;

  // This is what we want to remove. mTargetPath is discendant path of mDirPath.
  nsCOMPtr<nsIFile> mTargetPath;

  bool mRecursive;
  bool mReturnValue;
};

class RemoveTaskParent final : public FileSystemTaskParentBase
{
public:
  static already_AddRefed<RemoveTaskParent>
  Create(FileSystemBase* aFileSystem,
         const FileSystemRemoveParams& aParam,
         FileSystemRequestParent* aParent,
         ErrorResult& aRv);

  virtual void
  GetPermissionAccessType(nsCString& aAccess) const override;

protected:
  virtual FileSystemResponseValue
  GetSuccessRequestResult(ErrorResult& aRv) const override;

  virtual nsresult
  IOWork() override;

private:
  RemoveTaskParent(FileSystemBase* aFileSystem,
                   const FileSystemRemoveParams& aParam,
                   FileSystemRequestParent* aParent);

  // This path is the Directory::mFile.
  nsCOMPtr<nsIFile> mDirPath;

  // This is what we want to remove. mTargetPath is discendant path of mDirPath.
  nsCOMPtr<nsIFile> mTargetPath;

  bool mRecursive;
  bool mReturnValue;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_RemoveTask_h
