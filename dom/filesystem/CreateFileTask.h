/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CreateFileTask_h
#define mozilla_dom_CreateFileTask_h

#include "mozilla/dom/FileSystemTaskBase.h"
#include "nsAutoPtr.h"
#include "mozilla/ErrorResult.h"

class nsIDOMBlob;
class nsIInputStream;

namespace mozilla {
namespace dom {

class DOMFileImpl;
class Promise;

class CreateFileTask MOZ_FINAL
  : public FileSystemTaskBase
{
public:
  CreateFileTask(FileSystemBase* aFileSystem,
                 const nsAString& aPath,
                 nsIDOMBlob* aBlobData,
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
  void
  GetOutputBufferSize() const;

  static uint32_t sOutputBufferSize;
  nsRefPtr<Promise> mPromise;
  nsString mTargetRealPath;
  nsCOMPtr<nsIDOMBlob> mBlobData;
  nsCOMPtr<nsIInputStream> mBlobStream;
  InfallibleTArray<uint8_t> mArrayData;
  bool mReplace;

  // This cannot be a DOMFile bacause this object is created on a different
  // thread and DOMFile is not thread-safe. Let's use the DOMFileImpl instead.
  nsRefPtr<DOMFileImpl> mTargetFileImpl;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CreateFileTask_h
