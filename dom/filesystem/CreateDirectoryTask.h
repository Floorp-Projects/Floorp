/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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

class CreateDirectoryTask MOZ_FINAL
  : public FileSystemTaskBase
{
public:
  CreateDirectoryTask(FileSystemBase* aFileSystem,
                      const nsAString& aPath,
                      ErrorResult& aRv);
  CreateDirectoryTask(FileSystemBase* aFileSystem,
                      const FileSystemCreateDirectoryParams& aParam,
                      FileSystemRequestParent* aParent);

  virtual
  ~CreateDirectoryTask();

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
  nsString mTargetRealPath;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CreateDirectoryTask_h
