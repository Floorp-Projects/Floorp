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
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIFile.h"
#include "nsIGlobalObject.h"
#include "nsLocalFile.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "xpcpublic.h"

namespace mozilla::dom {

static constexpr auto ERROR_EMPTY_PATH =
    "PathUtils does not support empty paths"_ns;
static constexpr auto ERROR_INITIALIZE_PATH = "Could not initialize path"_ns;
static constexpr auto ERROR_GET_PARENT = "Could not get parent path"_ns;
static constexpr auto ERROR_JOIN = "Could not append to path"_ns;

static constexpr auto COLON = ": "_ns;

static void ThrowError(ErrorResult& aErr, const nsresult aResult,
                       const nsCString& aMessage) {
  nsAutoCStringN<32> errName;
  GetErrorName(aResult, errName);

  nsAutoCStringN<256> formattedMsg;
  formattedMsg.Append(aMessage);
  formattedMsg.Append(COLON);
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

static bool DoWindowsPathCheck() {
#ifdef XP_WIN
#  ifdef DEBUG
  return true;
#  else   // DEBUG
  return xpc::IsInAutomation();
#  endif  // DEBUG
#else     // XP_WIN
  return false;
#endif    // XP_WIN
}

/* static */
nsresult PathUtils::InitFileWithPath(nsIFile* aFile, const nsAString& aPath) {
  if (DoWindowsPathCheck()) {
    MOZ_RELEASE_ASSERT(!aPath.Contains(u'/'),
                       "Windows paths cannot include forward slashes");
  }

  MOZ_ASSERT(aFile);
  return aFile->InitWithPath(aPath);
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
  if (nsresult rv = InitFileWithPath(path, aPath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  if (nsresult rv = GetLeafNamePreservingRoot(path, aResult); NS_FAILED(rv)) {
    ThrowError(aErr, rv, "Could not get leaf name of path"_ns);
    return;
  }
}

void PathUtils::Parent(const GlobalObject&, const nsAString& aPath,
                       const int32_t aDepth, nsString& aResult,
                       ErrorResult& aErr) {
  if (aPath.IsEmpty()) {
    aErr.ThrowNotAllowedError(ERROR_EMPTY_PATH);
    return;
  }

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = InitFileWithPath(path, aPath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  if (aDepth <= 0) {
    aErr.ThrowNotSupportedError("A depth of at least 1 is required");
    return;
  }

  nsCOMPtr<nsIFile> parent;
  for (int32_t i = 0; path && i < aDepth; i++) {
    if (nsresult rv = path->GetParent(getter_AddRefs(parent)); NS_FAILED(rv)) {
      ThrowError(aErr, rv, ERROR_GET_PARENT);
      return;
    }
    path = parent;
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
  if (nsresult rv = InitFileWithPath(path, aComponents[0]); NS_FAILED(rv)) {
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
  if (nsresult rv = InitFileWithPath(path, aBasePath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  if (nsresult rv = path->AppendRelativePath(aRelativePath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_JOIN);
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
  if (nsresult rv = InitFileWithPath(path, isUNC ? uncPath : normalPath);
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
  if (nsresult rv = InitFileWithPath(path, aPath); NS_FAILED(rv)) {
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
  if (nsresult rv = InitFileWithPath(path, aPath); NS_FAILED(rv)) {
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
  if (nsresult rv = InitFileWithPath(path, aPath); NS_FAILED(rv)) {
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

bool PathUtils::IsAbsolute(const GlobalObject&, const nsAString& aPath) {
  nsCOMPtr<nsIFile> path = new nsLocalFile();
  nsresult rv = InitFileWithPath(path, aPath);
  return NS_SUCCEEDED(rv);
}

void PathUtils::GetProfileDirSync(const GlobalObject&, nsString& aResult,
                                  ErrorResult& aErr) {
  MOZ_ASSERT(NS_IsMainThread());

  auto guard = sDirCache.Lock();
  DirectoryCache::Ensure(guard.ref())
      .GetDirectorySync(aResult, aErr, DirectoryCache::Directory::Profile);
}
void PathUtils::GetLocalProfileDirSync(const GlobalObject&, nsString& aResult,
                                       ErrorResult& aErr) {
  MOZ_ASSERT(NS_IsMainThread());

  auto guard = sDirCache.Lock();
  DirectoryCache::Ensure(guard.ref())
      .GetDirectorySync(aResult, aErr, DirectoryCache::Directory::LocalProfile);
}
void PathUtils::GetTempDirSync(const GlobalObject&, nsString& aResult,
                               ErrorResult& aErr) {
  MOZ_ASSERT(NS_IsMainThread());

  auto guard = sDirCache.Lock();
  DirectoryCache::Ensure(guard.ref())
      .GetDirectorySync(aResult, aErr, DirectoryCache::Directory::Temp);
}

void PathUtils::GetOSTempDirSync(const GlobalObject&, nsString& aResult,
                                 ErrorResult& aErr) {
  MOZ_ASSERT(NS_IsMainThread());

  auto guard = sDirCache.Lock();
  DirectoryCache::Ensure(guard.ref())
      .GetDirectorySync(aResult, aErr, DirectoryCache::Directory::OSTemp);
}

void PathUtils::GetXulLibraryPathSync(const GlobalObject&, nsString& aResult,
                                      ErrorResult& aErr) {
  MOZ_ASSERT(NS_IsMainThread());

  auto guard = sDirCache.Lock();
  DirectoryCache::Ensure(guard.ref())
      .GetDirectorySync(aResult, aErr, DirectoryCache::Directory::XulLibrary);
}

already_AddRefed<Promise> PathUtils::GetProfileDirAsync(
    const GlobalObject& aGlobal, ErrorResult& aErr) {
  MOZ_ASSERT(!NS_IsMainThread());

  auto guard = sDirCache.Lock();
  return DirectoryCache::Ensure(guard.ref())
      .GetDirectoryAsync(aGlobal, aErr, DirectoryCache::Directory::Profile);
}

already_AddRefed<Promise> PathUtils::GetLocalProfileDirAsync(
    const GlobalObject& aGlobal, ErrorResult& aErr) {
  MOZ_ASSERT(!NS_IsMainThread());

  auto guard = sDirCache.Lock();
  return DirectoryCache::Ensure(guard.ref())
      .GetDirectoryAsync(aGlobal, aErr,
                         DirectoryCache::Directory::LocalProfile);
}

already_AddRefed<Promise> PathUtils::GetTempDirAsync(
    const GlobalObject& aGlobal, ErrorResult& aErr) {
  MOZ_ASSERT(!NS_IsMainThread());

  auto guard = sDirCache.Lock();
  return DirectoryCache::Ensure(guard.ref())
      .GetDirectoryAsync(aGlobal, aErr, DirectoryCache::Directory::Temp);
}

already_AddRefed<Promise> PathUtils::GetOSTempDirAsync(
    const GlobalObject& aGlobal, ErrorResult& aErr) {
  MOZ_ASSERT(!NS_IsMainThread());

  auto guard = sDirCache.Lock();
  return DirectoryCache::Ensure(guard.ref())
      .GetDirectoryAsync(aGlobal, aErr, DirectoryCache::Directory::OSTemp);
}

already_AddRefed<Promise> PathUtils::GetXulLibraryPathAsync(
    const GlobalObject& aGlobal, ErrorResult& aErr) {
  MOZ_ASSERT(!NS_IsMainThread());

  auto guard = sDirCache.Lock();
  return DirectoryCache::Ensure(guard.ref())
      .GetDirectoryAsync(aGlobal, aErr, DirectoryCache::Directory::XulLibrary);
}

PathUtils::DirectoryCache::DirectoryCache() {
  for (auto& dir : mDirectories) {
    dir.SetIsVoid(true);
  }
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

void PathUtils::DirectoryCache::GetDirectorySync(
    nsString& aResult, ErrorResult& aErr, const Directory aRequestedDir) {
  MOZ_RELEASE_ASSERT(aRequestedDir < Directory::Count);

  if (nsresult rv = PopulateDirectoriesImpl(aRequestedDir); NS_FAILED(rv)) {
    nsAutoCStringN<32> errorName;
    GetErrorName(rv, errorName);

    nsAutoCStringN<256> msg;
    msg.Append("Could not retrieve directory "_ns);
    msg.Append(kDirectoryNames[aRequestedDir]);
    msg.Append(COLON);
    msg.Append(errorName);

    aErr.ThrowUnknownError(msg);
    return;
  }

  aResult = mDirectories[aRequestedDir];
}

already_AddRefed<Promise> PathUtils::DirectoryCache::GetDirectoryAsync(
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
  MOZ_RELEASE_ASSERT(aRequestedDir < Directory::Count);
  MOZ_RELEASE_ASSERT(!mDirectories[aRequestedDir].IsVoid());
  aPromise->MaybeResolve(mDirectories[aRequestedDir]);
}

already_AddRefed<PathUtils::DirectoryCache::PopulateDirectoriesPromise>
PathUtils::DirectoryCache::PopulateDirectories(
    const PathUtils::DirectoryCache::Directory aRequestedDir) {
  MOZ_RELEASE_ASSERT(aRequestedDir < Directory::Count);

  // If we have already resolved the requested directory, we can return
  // immediately.
  // Otherwise, if we have already fired off a request to populate the entry,
  // so we can return the corresponding promise immediately. caller will queue
  // a Thenable onto that promise to resolve/reject the request.
  if (!mDirectories[aRequestedDir].IsVoid()) {
    return nullptr;
  }
  if (!mPromises[aRequestedDir].IsEmpty()) {
    return mPromises[aRequestedDir].Ensure(__func__);
  }

  RefPtr<PopulateDirectoriesPromise> promise =
      mPromises[aRequestedDir].Ensure(__func__);

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
  MOZ_RELEASE_ASSERT(aRequestedDir < Directory::Count);

  if (NS_SUCCEEDED(aRv)) {
    mPromises[aRequestedDir].Resolve(Ok{}, __func__);
  } else {
    mPromises[aRequestedDir].Reject(aRv, __func__);
  }
}

nsresult PathUtils::DirectoryCache::PopulateDirectoriesImpl(
    const PathUtils::DirectoryCache::Directory aRequestedDir) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(aRequestedDir < Directory::Count);

  if (!mDirectories[aRequestedDir].IsVoid()) {
    // In between when this promise was dispatched to the main thread and now,
    // the directory cache has had this entry populated (via the
    // on-main-thread sync method).
    return NS_OK;
  }

  nsCOMPtr<nsIFile> path;

  MOZ_TRY(NS_GetSpecialDirectory(kDirectoryNames[aRequestedDir],
                                 getter_AddRefs(path)));
  MOZ_TRY(path->GetPath(mDirectories[aRequestedDir]));

  return NS_OK;
}

}  // namespace mozilla::dom
