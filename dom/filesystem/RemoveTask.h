/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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

class Promise;

class RemoveTask MOZ_FINAL
  : public FileSystemTaskBase
{
public:
  RemoveTask(FileSystemBase* aFileSystem,
             const nsAString& aDirPath,
             nsIDOMFile* aTargetFile,
             const nsAString& aTargetPath,
             bool aRecursive,
             ErrorResult& aRv);
  RemoveTask(FileSystemBase* aFileSystem,
             const FileSystemRemoveParams& aParam,
             FileSystemRequestParent* aParent);

  virtual
  ~RemoveTask();

  already_AddRefed<Promise>
  GetPromise();

  virtual void
  GetPermissionAccessType(nsCString& aAccess) const MOZ_OVERRIDE;

protected:
  virtual FileSystemParams
  GetRequestParams(const nsString& aFileSystem) const MOZ_OVERRIDE;

  virtual FileSystemResponseValue
  GetSuccessRequestResult() const MOZ_OVERRIDE;

  virtual void
  SetSuccessRequestResult(const FileSystemResponseValue& aValue) MOZ_OVERRIDE;

  virtual nsresult
  Work() MOZ_OVERRIDE;

  virtual void
  HandlerCallback() MOZ_OVERRIDE;

private:
  nsRefPtr<Promise> mPromise;
  nsString mDirRealPath;
  nsCOMPtr<nsIDOMFile> mTargetFile;
  nsString mTargetRealPath;
  bool mRecursive;
  bool mReturnValue;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_RemoveTask_h
