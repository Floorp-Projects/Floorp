/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CreateDirectoryTask_h
#define mozilla_dom_CreateDirectoryTask_h

#include "mozilla/dom/FileSystemTaskBase.h"
#include "nsAutoPtr.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {

class Promise;

class CreateDirectoryTask final : public FileSystemTaskBase
{
public:
  static already_AddRefed<CreateDirectoryTask>
  Create(FileSystemBase* aFileSystem,
         nsIFile* aTargetPath,
         ErrorResult& aRv);

  static already_AddRefed<CreateDirectoryTask>
  Create(FileSystemBase* aFileSystem,
         const FileSystemCreateDirectoryParams& aParam,
         FileSystemRequestParent* aParent,
         ErrorResult& aRv);

  virtual
  ~CreateDirectoryTask();

  already_AddRefed<Promise>
  GetPromise();

  virtual void
  GetPermissionAccessType(nsCString& aAccess) const override;

protected:
  virtual FileSystemParams
  GetRequestParams(const nsString& aSerializedDOMPath,
                   ErrorResult& aRv) const override;

  virtual FileSystemResponseValue
  GetSuccessRequestResult(ErrorResult& aRv) const override;

  virtual void
  SetSuccessRequestResult(const FileSystemResponseValue& aValue,
                          ErrorResult& aRv) override;

  virtual nsresult
  Work() override;

  virtual void
  HandlerCallback() override;

private:
  CreateDirectoryTask(FileSystemBase* aFileSystem,
                      nsIFile* aTargetPath);

  CreateDirectoryTask(FileSystemBase* aFileSystem,
                      const FileSystemCreateDirectoryParams& aParam,
                      FileSystemRequestParent* aParent);

  RefPtr<Promise> mPromise;
  nsCOMPtr<nsIFile> mTargetPath;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CreateDirectoryTask_h
