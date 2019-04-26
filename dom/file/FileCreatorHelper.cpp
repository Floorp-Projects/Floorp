/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileCreatorHelper.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/FileCreatorChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/StaticPrefs.h"
#include "nsContentUtils.h"
#include "nsPIDOMWindow.h"
#include "nsProxyRelease.h"
#include "nsIFile.h"

// Undefine the macro of CreateFile to avoid FileCreatorHelper#CreateFile being
// replaced by FileCreatorHelper#CreateFileW.
#ifdef CreateFile
#  undef CreateFile
#endif

namespace mozilla {
namespace dom {

/* static */
already_AddRefed<Promise> FileCreatorHelper::CreateFile(
    nsIGlobalObject* aGlobalObject, nsIFile* aFile,
    const ChromeFilePropertyBag& aBag, bool aIsFromNsIFile, ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  RefPtr<Promise> promise = Promise::Create(aGlobalObject, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsAutoString path;
  aRv = aFile->GetPath(path);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Register this component to PBackground.
  mozilla::ipc::PBackgroundChild* actorChild =
      mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  Maybe<int64_t> lastModified;
  if (aBag.mLastModified.WasPassed()) {
    lastModified.emplace(aBag.mLastModified.Value());
  }

  PFileCreatorChild* actor = actorChild->SendPFileCreatorConstructor(
      path, aBag.mType, aBag.mName, lastModified, aBag.mExistenceCheck,
      aIsFromNsIFile);

  static_cast<FileCreatorChild*>(actor)->SetPromise(promise);
  return promise.forget();
}

}  // namespace dom
}  // namespace mozilla
