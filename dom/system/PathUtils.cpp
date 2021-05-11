/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "PathUtils.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DataMutex.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Span.h"
#include "mozilla/dom/DOMParser.h"
#include "mozilla/dom/PathUtilsBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIFile.h"
#include "nsIGlobalObject.h"
#include "nsLocalFile.h"
#include "nsNetUtil.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

static constexpr auto ERROR_EMPTY_PATH =
    "PathUtils does not support empty paths"_ns;
static constexpr auto ERROR_INITIALIZE_PATH = "Could not initialize path"_ns;
static constexpr auto ERROR_GET_PARENT = "Could not get parent path"_ns;
static constexpr auto ERROR_JOIN = "Could not append to path"_ns;
static constexpr auto ERROR_CREATE_UNIQUE = "Could not create unique path"_ns;

static void ThrowError(ErrorResult& aErr, const nsresult aResult,
                       const nsCString& aMessage) {
  nsAutoCStringN<32> errName;
  GetErrorName(aResult, errName);

  nsAutoCStringN<256> formattedMsg;
  formattedMsg.Append(aMessage);
  formattedMsg.Append(": "_ns);
  formattedMsg.Append(errName);

  switch (aResult) {
    case NS_ERROR_FILE_UNRECOGNIZED_PATH:
      aErr.ThrowOperationError(formattedMsg);
      break;

    case NS_ERROR_FILE_ACCESS_DENIED:
      aErr.ThrowInvalidAccessError(formattedMsg);
      break;

    case NS_ERROR_FAILURE:
    default:
      aErr.ThrowUnknownError(formattedMsg);
      break;
  }
}

StaticDataMutex<Maybe<PathUtils::DirectoryCache>> PathUtils::sDirCache{
    "sDirCache"};

/**
 * Return the leaf name, including leading path separators in the case of
 * Windows UNC drive paths.
 *
 * @param aFile The file whose leaf name is to be returned.
 * @param aResult The string to hold the resulting leaf name.
 * @param aParent The pre-computed parent of |aFile|. If not provided, it will
 *                be computed.
 */
static nsresult GetLeafNamePreservingRoot(nsIFile* aFile, nsString& aResult,
                                          nsIFile* aParent = nullptr) {
  MOZ_ASSERT(aFile);

  nsCOMPtr<nsIFile> parent = aParent;
  if (!parent) {
    MOZ_TRY(aFile->GetParent(getter_AddRefs(parent)));
  }

  if (parent) {
    return aFile->GetLeafName(aResult);
  }

  // We have reached the root path. On Windows, the leafname for a UNC path
  // will not have the leading backslashes, so we need to use the entire path
  // here:
  //
  // * for a UNIX root path (/) this will be /;
  // * for a Windows drive path (e.g., C:), this will be the drive path (C:);
  //   and
  // * for a Windows UNC server path (e.g., \\\\server), this will be the full
  //   server path (\\\\server).
  return aFile->GetPath(aResult);
}

void PathUtils::Filename(const GlobalObject&, const nsAString& aPath,
                         nsString& aResult, ErrorResult& aErr) {
  if (aPath.IsEmpty()) {
    aErr.ThrowNotAllowedError(ERROR_EMPTY_PATH);
    return;
  }

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = path->InitWithPath(aPath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  if (nsresult rv = GetLeafNamePreservingRoot(path, aResult); NS_FAILED(rv)) {
    ThrowError(aErr, rv, "Could not get leaf name of path"_ns);
    return;
  }
}

void PathUtils::Parent(const GlobalObject&, const nsAString& aPath,
                       nsString& aResult, ErrorResult& aErr) {
  if (aPath.IsEmpty()) {
    aErr.ThrowNotAllowedError(ERROR_EMPTY_PATH);
    return;
  }

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = path->InitWithPath(aPath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  nsCOMPtr<nsIFile> parent;
  if (nsresult rv = path->GetParent(getter_AddRefs(parent)); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_GET_PARENT);
    return;
  }

  if (parent) {
    MOZ_ALWAYS_SUCCEEDS(parent->GetPath(aResult));
  } else {
    aResult = VoidString();
  }
}

void PathUtils::Join(const GlobalObject&, const Sequence<nsString>& aComponents,
                     nsString& aResult, ErrorResult& aErr) {
  if (aComponents.IsEmpty()) {
    return;
  }
  if (aComponents[0].IsEmpty()) {
    aErr.ThrowNotAllowedError(ERROR_EMPTY_PATH);
    return;
  }

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = path->InitWithPath(aComponents[0]); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  const auto components = Span<const nsString>(aComponents).Subspan(1);
  for (const auto& component : components) {
    if (nsresult rv = path->Append(component); NS_FAILED(rv)) {
      ThrowError(aErr, rv, ERROR_JOIN);
      return;
    }
  }

  MOZ_ALWAYS_SUCCEEDS(path->GetPath(aResult));
}

void PathUtils::JoinRelative(const GlobalObject&, const nsAString& aBasePath,
                             const nsAString& aRelativePath, nsString& aResult,
                             ErrorResult& aErr) {
  if (aRelativePath.IsEmpty()) {
    aResult = aBasePath;
    return;
  }

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = path->InitWithPath(aBasePath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  if (nsresult rv = path->AppendRelativePath(aRelativePath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_JOIN);
    return;
  }

  MOZ_ALWAYS_SUCCEEDS(path->GetPath(aResult));
}

void PathUtils::CreateUniquePath(const GlobalObject&, const nsAString& aPath,
                                 nsString& aResult, ErrorResult& aErr) {
  if (aPath.IsEmpty()) {
    aErr.ThrowNotAllowedError(ERROR_EMPTY_PATH);
    return;
  }

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = path->InitWithPath(aPath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  if (nsresult rv = path->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
      NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_CREATE_UNIQUE);
    return;
  }

  MOZ_ALWAYS_SUCCEEDS(path->GetPath(aResult));
}

void PathUtils::ToExtendedWindowsPath(const GlobalObject&,
                                      const nsAString& aPath, nsString& aResult,
                                      ErrorResult& aErr) {
#ifndef XP_WIN
  aErr.ThrowNotAllowedError("Operation is windows specific"_ns);
  return;
#else
  if (aPath.IsEmpty()) {
    aErr.ThrowNotAllowedError(ERROR_EMPTY_PATH);
    return;
  }

  const nsAString& str1 = Substring(aPath, 1, 1);
  const nsAString& str2 = Substring(aPath, 2, aPath.Length() - 2);

  bool isUNC = aPath.Length() >= 2 &&
               (aPath.First() == '\\' || aPath.First() == '/') &&
               (str1.EqualsLiteral("\\") || str1.EqualsLiteral("/"));

  constexpr auto pathPrefix = u"\\\\?\\"_ns;
  const nsAString& uncPath = pathPrefix + u"UNC\\"_ns + str2;
  const nsAString& normalPath = pathPrefix + aPath;

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = path->InitWithPath(isUNC ? uncPath : normalPath);
      NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  MOZ_ALWAYS_SUCCEEDS(path->GetPath(aResult));
#endif
}

void PathUtils::Normalize(const GlobalObject&, const nsAString& aPath,
                          nsString& aResult, ErrorResult& aErr) {
  if (aPath.IsEmpty()) {
    aErr.ThrowNotAllowedError(ERROR_EMPTY_PATH);
    return;
  }

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = path->InitWithPath(aPath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  if (nsresult rv = path->Normalize(); NS_FAILED(rv)) {
    ThrowError(aErr, rv, "Could not normalize path"_ns);
    return;
  }

  MOZ_ALWAYS_SUCCEEDS(path->GetPath(aResult));
}

void PathUtils::Split(const GlobalObject&, const nsAString& aPath,
                      nsTArray<nsString>& aResult, ErrorResult& aErr) {
  if (aPath.IsEmpty()) {
    aErr.ThrowNotAllowedError(ERROR_EMPTY_PATH);
    return;
  }

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = path->InitWithPath(aPath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  while (path) {
    auto* component = aResult.EmplaceBack(fallible);
    if (!component) {
      aErr.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    nsCOMPtr<nsIFile> parent;
    if (nsresult rv = path->GetParent(getter_AddRefs(parent)); NS_FAILED(rv)) {
      ThrowError(aErr, rv, ERROR_GET_PARENT);
      return;
    }

    // GetLeafPreservingRoot cannot fail if we pass it a parent path.
    MOZ_ALWAYS_SUCCEEDS(GetLeafNamePreservingRoot(path, *component, parent));

    path = parent;
  }

  aResult.Reverse();
}

void PathUtils::ToFileURI(const GlobalObject&, const nsAString& aPath,
                          nsCString& aResult, ErrorResult& aErr) {
  if (aPath.IsEmpty()) {
    aErr.ThrowNotAllowedError(ERROR_EMPTY_PATH);
    return;
  }

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = path->InitWithPath(aPath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  nsCOMPtr<nsIURI> uri;
  if (nsresult rv = NS_NewFileURI(getter_AddRefs(uri), path); NS_FAILED(rv)) {
    ThrowError(aErr, rv, "Could not initialize File URI"_ns);
    return;
  }

  if (nsresult rv = uri->GetSpec(aResult); NS_FAILED(rv)) {
    ThrowError(aErr, rv, "Could not retrieve URI spec"_ns);
    return;
  }
}

already_AddRefed<Promise> PathUtils::GetProfileDir(const GlobalObject& aGlobal,
                                                   ErrorResult& aErr) {
  auto guard = sDirCache.Lock();
  return DirectoryCache::Ensure(guard.ref())
      .GetDirectory(aGlobal, aErr, DirectoryCache::Directory::Profile);
}

already_AddRefed<Promise> PathUtils::GetLocalProfileDir(
    const GlobalObject& aGlobal, ErrorResult& aErr) {
  auto guard = sDirCache.Lock();
  return DirectoryCache::Ensure(guard.ref())
      .GetDirectory(aGlobal, aErr, DirectoryCache::Directory::LocalProfile);
}

already_AddRefed<Promise> PathUtils::GetTempDir(const GlobalObject& aGlobal,
                                                ErrorResult& aErr) {
  auto guard = sDirCache.Lock();
  return DirectoryCache::Ensure(guard.ref())
      .GetDirectory(aGlobal, aErr, DirectoryCache::Directory::Temp);
}

PathUtils::DirectoryCache::DirectoryCache() {
  mProfileDir.SetIsVoid(true);
  mLocalProfileDir.SetIsVoid(true);
  mTempDir.SetIsVoid(true);
}

PathUtils::DirectoryCache& PathUtils::DirectoryCache::Ensure(
    Maybe<PathUtils::DirectoryCache>& aCache) {
  if (aCache.isNothing()) {
    aCache.emplace();

    auto clearAtShutdown = []() {
      RunOnShutdown([]() {
        auto cache = PathUtils::sDirCache.Lock();
        cache->reset();
      });
    };

    if (NS_IsMainThread()) {
      clearAtShutdown();
    } else {
      NS_DispatchToMainThread(
          NS_NewRunnableFunction(__func__, std::move(clearAtShutdown)));
    }
  }

  return aCache.ref();
}

already_AddRefed<Promise> PathUtils::DirectoryCache::GetDirectory(
    const GlobalObject& aGlobal, ErrorResult& aErr,
    const Directory aRequestedDir) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<Promise> promise = Promise::Create(global, aErr);
  if (aErr.Failed()) {
    return nullptr;
  }

  if (RefPtr<PopulateDirectoriesPromise> p =
          PopulateDirectories(aRequestedDir)) {
    p->Then(
        GetCurrentSerialEventTarget(), __func__,
        [promise, aRequestedDir](const Ok&) {
          auto cache = PathUtils::sDirCache.Lock();
          cache.ref()->ResolveWithDirectory(promise, aRequestedDir);
        },
        [promise](const nsresult& aRv) { promise->MaybeReject(aRv); });
  } else {
    ResolveWithDirectory(promise, aRequestedDir);
  }

  return promise.forget();
}

void PathUtils::DirectoryCache::ResolveWithDirectory(
    Promise* aPromise, const Directory aRequestedDir) {
  switch (aRequestedDir) {
    case Directory::Profile:
      MOZ_RELEASE_ASSERT(!mProfileDir.IsVoid());
      aPromise->MaybeResolve(mProfileDir);
      break;

    case Directory::LocalProfile:
      MOZ_RELEASE_ASSERT(!mLocalProfileDir.IsVoid());
      aPromise->MaybeResolve(mLocalProfileDir);
      break;

    case Directory::Temp:
      MOZ_RELEASE_ASSERT(!mTempDir.IsVoid());
      aPromise->MaybeResolve(mTempDir);
      break;

    default:
      MOZ_ASSERT_UNREACHABLE();
  }
}

already_AddRefed<PathUtils::DirectoryCache::PopulateDirectoriesPromise>
PathUtils::DirectoryCache::PopulateDirectories(
    const PathUtils::DirectoryCache::Directory aRequestedDir) {
  // If we have already resolved the requested directory, we can return
  // immediately.
  if ((aRequestedDir == Directory::Temp && !mTempDir.IsVoid()) ||
      (aRequestedDir == Directory::Profile && !mProfileDir.IsVoid()) ||
      (aRequestedDir == Directory::LocalProfile &&
       !mLocalProfileDir.IsVoid())) {
    // We cannot have a state where mProfileDir is not populated but
    // mLocalProfileDir is.
    if (mProfileDir.IsVoid()) {
      MOZ_RELEASE_ASSERT(mLocalProfileDir.IsVoid());
    }
    return nullptr;
  }

  // We have already fired off a request to populate the entry, so we can return
  // the corresponding promise immediately. caller will queue a Thenable onto
  // that promise to resolve/reject the request.
  if (!mAllDirsPromise.IsEmpty()) {
    return mAllDirsPromise.Ensure(__func__);
  }
  if (aRequestedDir != Directory::Temp && !mProfileDirsPromise.IsEmpty()) {
    return mProfileDirsPromise.Ensure(__func__);
  }

  RefPtr<PopulateDirectoriesPromise> promise;
  if (aRequestedDir == Directory::Temp) {
    promise = mAllDirsPromise.Ensure(__func__);
  } else {
    promise = mProfileDirsPromise.Ensure(__func__);
  }

  if (NS_IsMainThread()) {
    nsresult rv = PopulateDirectoriesImpl(aRequestedDir);
    ResolvePopulateDirectoriesPromise(rv, aRequestedDir);
  } else {
    nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableFunction(__func__, [aRequestedDir]() {
          auto cache = PathUtils::sDirCache.Lock();
          nsresult rv = cache.ref()->PopulateDirectoriesImpl(aRequestedDir);
          cache.ref()->ResolvePopulateDirectoriesPromise(rv, aRequestedDir);
        });
    NS_DispatchToMainThread(runnable.forget());
  }

  return promise.forget();
}

void PathUtils::DirectoryCache::ResolvePopulateDirectoriesPromise(
    nsresult aRv, const PathUtils::DirectoryCache::Directory aRequestedDir) {
  if (NS_SUCCEEDED(aRv)) {
    if (aRequestedDir == Directory::Temp) {
      mAllDirsPromise.Resolve(Ok{}, __func__);
    } else {
      mProfileDirsPromise.Resolve(Ok{}, __func__);
    }
  } else {
    if (aRequestedDir == Directory::Temp) {
      mAllDirsPromise.Reject(aRv, __func__);
    } else {
      mProfileDirsPromise.Reject(aRv, __func__);
    }
  }
}

nsresult PathUtils::DirectoryCache::PopulateDirectoriesImpl(
    const PathUtils::DirectoryCache::Directory aRequestedDir) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIFile> path;

  // We only populate the temporary directory entry when specifically requested
  // because the nsDirectoryService will do main thread IO to create the
  // directory if it hasn't been created yet.
  //
  // Additionally, we cannot have second request to populate any of these
  // directories if the first request succeeded, so assert that the
  // corresponding fields are void.
  if (aRequestedDir == Directory::Temp) {
    MOZ_RELEASE_ASSERT(mTempDir.IsVoid());

    MOZ_TRY(NS_GetSpecialDirectory(NS_APP_CONTENT_PROCESS_TEMP_DIR,
                                   getter_AddRefs(path)));
    MOZ_TRY(path->GetPath(mTempDir));
  } else if (aRequestedDir == Directory::Profile) {
    MOZ_RELEASE_ASSERT(mProfileDir.IsVoid());
    MOZ_RELEASE_ASSERT(mLocalProfileDir.IsVoid());
  } else {
    MOZ_RELEASE_ASSERT(aRequestedDir == Directory::LocalProfile);
    MOZ_RELEASE_ASSERT(mProfileDir.IsVoid());
    MOZ_RELEASE_ASSERT(mLocalProfileDir.IsVoid());
  }

  if (mProfileDir.IsVoid()) {
    MOZ_RELEASE_ASSERT(mLocalProfileDir.IsVoid());

    nsString profileDir;
    nsString localProfileDir;

    MOZ_TRY(NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                   getter_AddRefs(path)));
    MOZ_TRY(path->GetPath(profileDir));

    MOZ_TRY(NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR,
                                   getter_AddRefs(path)));
    MOZ_TRY(path->GetPath(localProfileDir));

    // We either set both of these or neither.
    mProfileDir = std::move(profileDir);
    mLocalProfileDir = std::move(localProfileDir);
  }

  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
