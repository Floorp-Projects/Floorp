/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GetDirectoryListing_h
#define mozilla_dom_GetDirectoryListing_h

#include "mozilla/dom/FileSystemTaskBase.h"
#include "mozilla/ErrorResult.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace dom {

class BlobImpl;

class GetDirectoryListingTask final
  : public FileSystemTaskBase
{
public:
  // If aDirectoryOnly is set, we should ensure that the target is a directory.
  GetDirectoryListingTask(FileSystemBase* aFileSystem,
                          const nsAString& aTargetPath,
                          const nsAString& aFilters,
                          ErrorResult& aRv);
  GetDirectoryListingTask(FileSystemBase* aFileSystem,
                          const FileSystemGetDirectoryListingParams& aParam,
                          FileSystemRequestParent* aParent);

  virtual
  ~GetDirectoryListingTask();

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
  RefPtr<Promise> mPromise;
  nsString mTargetRealPath;
  nsString mFilters;

  // We cannot store File or Directory objects bacause this object is created
  // on a different thread and File and Directory are not thread-safe.
  nsTArray<RefPtr<BlobImpl>> mTargetBlobImpls;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_GetDirectoryListing_h
