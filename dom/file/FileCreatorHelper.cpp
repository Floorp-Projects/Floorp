/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileCreatorHelper.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/Promise.h"
#include "nsContentUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIFile.h"

// Undefine the macro of CreateFile to avoid FileCreatorHelper#CreateFile being
// replaced by FileCreatorHelper#CreateFileW.
#ifdef CreateFile
#undef CreateFile
#endif

namespace mozilla {
namespace dom {

/* static */ already_AddRefed<Promise>
FileCreatorHelper::CreateFile(nsIGlobalObject* aGlobalObject,
                              nsIFile* aFile,
                              const ChromeFilePropertyBag& aBag,
                              bool aIsFromNsIFile,
                              ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Promise> promise = Promise::Create(aGlobalObject, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobalObject);

  // Parent process

  if (XRE_IsParentProcess()) {
    RefPtr<File> file =
      CreateFileInternal(window, aFile, aBag, aIsFromNsIFile, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    promise->MaybeResolve(file);
    return promise.forget();
  }

  // Content process.

  RefPtr<FileCreatorHelper> helper = new FileCreatorHelper(promise, window);

  // The request is sent to the parent process and it's kept alive by
  // ContentChild.
  helper->SendRequest(aFile, aBag, aIsFromNsIFile, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return promise.forget();
}

/* static */ already_AddRefed<File>
FileCreatorHelper::CreateFileInternal(nsPIDOMWindowInner* aWindow,
                                      nsIFile* aFile,
                                      const ChromeFilePropertyBag& aBag,
                                      bool aIsFromNsIFile,
                                      ErrorResult& aRv)
{
  bool lastModifiedPassed = false;
  int64_t lastModified = 0;
  if (aBag.mLastModified.WasPassed()) {
    lastModifiedPassed = true;
    lastModified = aBag.mLastModified.Value();
  }

  RefPtr<BlobImpl> blobImpl;
  aRv = CreateBlobImpl(aFile, aBag.mType, aBag.mName, lastModifiedPassed,
                       lastModified, aIsFromNsIFile, getter_AddRefs(blobImpl));
  if (aRv.Failed()) {
     return nullptr;
  }

  RefPtr<File> file = File::Create(aWindow, blobImpl);
  return file.forget();
}

FileCreatorHelper::FileCreatorHelper(Promise* aPromise,
                                     nsPIDOMWindowInner* aWindow)
  : mPromise(aPromise)
  , mWindow(aWindow)
{
  MOZ_ASSERT(aPromise);
}

FileCreatorHelper::~FileCreatorHelper()
{
}

void
FileCreatorHelper::SendRequest(nsIFile* aFile,
                               const ChromeFilePropertyBag& aBag,
                               bool aIsFromNsIFile,
                               ErrorResult& aRv)
{
  MOZ_ASSERT(aFile);

  ContentChild* cc = ContentChild::GetSingleton();
  if (NS_WARN_IF(!cc)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsID uuid;
  aRv = nsContentUtils::GenerateUUIDInPlace(uuid);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsAutoString path;
  aRv = aFile->GetPath(path);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  cc->FileCreationRequest(uuid, this, path, aBag.mType, aBag.mName,
                          aBag.mLastModified, aIsFromNsIFile);
}

void
FileCreatorHelper::ResponseReceived(BlobImpl* aBlobImpl, nsresult aRv)
{
  if (NS_FAILED(aRv)) {
    mPromise->MaybeReject(aRv);
    return;
  }

  RefPtr<File> file = File::Create(mWindow, aBlobImpl);
  mPromise->MaybeResolve(file);
}

/* static */ nsresult
FileCreatorHelper::CreateBlobImplForIPC(const nsAString& aPath,
                                        const nsAString& aType,
                                        const nsAString& aName,
                                        bool aLastModifiedPassed,
                                        int64_t aLastModified,
                                        bool aIsFromNsIFile,
                                        BlobImpl** aBlobImpl)
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_NewLocalFile(aPath, true, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return CreateBlobImpl(file, aType, aName, aLastModifiedPassed, aLastModified,
                        aIsFromNsIFile, aBlobImpl);
}

/* static */ nsresult
FileCreatorHelper::CreateBlobImpl(nsIFile* aFile,
                                  const nsAString& aType,
                                  const nsAString& aName,
                                  bool aLastModifiedPassed,
                                  int64_t aLastModified,
                                  bool aIsFromNsIFile,
                                  BlobImpl** aBlobImpl)
{
  RefPtr<MultipartBlobImpl> impl = new MultipartBlobImpl(EmptyString());
  nsresult rv =
    impl->InitializeChromeFile(aFile, aType, aName, aLastModifiedPassed,
                               aLastModified, aIsFromNsIFile);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_ASSERT(impl->IsFile());

  impl.forget(aBlobImpl);
  return NS_OK;
}

} // dom namespace
} // mozilla namespace
