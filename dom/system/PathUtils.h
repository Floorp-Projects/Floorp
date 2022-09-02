/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PathUtils__
#define mozilla_dom_PathUtils__

#include "mozilla/DataMutex.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Mutex.h"
#include "mozilla/Result.h"
#include "mozilla/dom/Promise.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class PathUtils final {
 public:
  /**
   * Initialize the given nsIFile with the given path.
   *
   * This is equivalent to calling nsIFile::InitWithPath() with the caveat that
   * on Windows debug or during Windows CI tests, we will crash if the path
   * contains a forward slash.
   *
   * @param aFile The file to initialize.
   * @param aPath The path to initialize the file with.
   *
   * @return The result of calling nsIFile::InitWithPath.
   */
  static nsresult InitFileWithPath(nsIFile* aFile, const nsAString& aPath);

  static void Filename(const GlobalObject&, const nsAString& aPath,
                       nsString& aResult, ErrorResult& aErr);

  static void Parent(const GlobalObject&, const nsAString& aPath,
                     const int32_t aDepth, nsString& aResult,
                     ErrorResult& aErr);

  static void Join(const GlobalObject&, const Sequence<nsString>& aComponents,
                   nsString& aResult, ErrorResult& aErr);

  static void JoinRelative(const GlobalObject&, const nsAString& aBasePath,
                           const nsAString& aRelativePath, nsString& aResult,
                           ErrorResult& aErr);

  static void ToExtendedWindowsPath(const GlobalObject&, const nsAString& aPath,
                                    nsString& aResult, ErrorResult& aErr);

  static void Normalize(const GlobalObject&, const nsAString& aPath,
                        nsString& aResult, ErrorResult& aErr);

  static void Split(const GlobalObject&, const nsAString& aPath,
                    nsTArray<nsString>& aResult, ErrorResult& aErr);

  static void ToFileURI(const GlobalObject&, const nsAString& aPath,
                        nsCString& aResult, ErrorResult& aErr);

  static bool IsAbsolute(const GlobalObject&, const nsAString& aPath);

  static void GetProfileDirSync(const GlobalObject&, nsString& aResult,
                                ErrorResult& aErr);
  static void GetLocalProfileDirSync(const GlobalObject&, nsString& aResult,
                                     ErrorResult& aErr);
  static void GetTempDirSync(const GlobalObject&, nsString& aResult,
                             ErrorResult& aErr);
  static void GetOSTempDirSync(const GlobalObject&, nsString& aResult,
                               ErrorResult& aErr);
  static void GetXulLibraryPathSync(const GlobalObject&, nsString& aResult,
                                    ErrorResult& aErr);

  static already_AddRefed<Promise> GetProfileDirAsync(
      const GlobalObject& aGlobal, ErrorResult& aErr);
  static already_AddRefed<Promise> GetLocalProfileDirAsync(
      const GlobalObject& aGlobal, ErrorResult& aErr);
  static already_AddRefed<Promise> GetTempDirAsync(const GlobalObject& aGlobal,
                                                   ErrorResult& aErr);
  static already_AddRefed<Promise> GetOSTempDirAsync(
      const GlobalObject& aGlobal, ErrorResult& aErr);
  static already_AddRefed<Promise> GetXulLibraryPathAsync(
      const GlobalObject& aGlobal, ErrorResult& aErr);

 private:
  class DirectoryCache;
  friend class DirectoryCache;

  static StaticDataMutex<Maybe<DirectoryCache>> sDirCache;
};

/**
 * A cache of commonly used directories
 */
class PathUtils::DirectoryCache final {
 public:
  /**
   * A directory that can be requested via |GetDirectorySync| or
   * |GetDirectoryAsync|.
   */
  enum class Directory {
    /**
     * The user's profile directory.
     */
    Profile,
    /**
     * The user's local profile directory.
     */
    LocalProfile,
    /**
     * The temporary directory for the process.
     */
    Temp,
    /**
     * The OS temporary directory.
     */
    OSTemp,
    /**
     * The libxul path.
     */
    XulLibrary,
    /**
     * The number of Directory entries.
     */
    Count,
  };

  DirectoryCache();
  DirectoryCache(const DirectoryCache&) = delete;
  DirectoryCache(DirectoryCache&&) = delete;
  DirectoryCache& operator=(const DirectoryCache&) = delete;
  DirectoryCache& operator=(DirectoryCache&&) = delete;

  /**
   * Ensure the cache is instantiated and schedule its destructor to run at
   * shutdown.
   *
   * If the cache is already instantiated, this is a no-op.
   *
   * @param aCache The cache to ensure is instantiated.
   */
  static DirectoryCache& Ensure(Maybe<DirectoryCache>& aCache);

  void GetDirectorySync(nsString& aResult, ErrorResult& aErr,
                        const Directory aRequestedDir);

  /**
   * Request the path of a specific directory.
   *
   * If the directory has not been requested before, this may require a trip to
   * the main thread to retrieve its path.
   *
   * @param aGlobalObject The JavaScript global.
   * @param aErr The error result.
   * @param aRequestedDir The directory for which the path is to be retrieved.
   *
   * @return A promise that resolves to the path of the requested directory.
   */
  already_AddRefed<Promise> GetDirectoryAsync(const GlobalObject& aGlobalObject,
                                              ErrorResult& aErr,
                                              const Directory aRequestedDir);

 private:
  using PopulateDirectoriesPromise = MozPromise<Ok, nsresult, false>;

  /**
   * Populate the directory cache entry for the requested directory.
   *
   * @param aRequestedDir The directory cache entry that was requested via
   *                      |GetDirectory|.
   *
   * @return If the requested directory has not been populated, this returns a
   *         promise that resolves when the population is complete.
   *
   *         If the requested directory has already been populated, it returns
   *         nullptr instead.
   */
  already_AddRefed<PopulateDirectoriesPromise> PopulateDirectories(
      const Directory aRequestedDir);

  /**
   * Initialize the requested directory cache entry.
   *
   * If |Directory::Temp| is requested, all cache entries will be populated.
   * Otherwise, only the profile and local profile cache entries will be
   * populated. The profile and local profile cache entries have no additional
   * overhead for populating them, but the temp directory requires creating a
   * directory on the main thread if it has not already happened.
   *
   * Must be called on the main thread.
   *
   * @param aRequestedDir The requested directory.
   *
   * @return The result of initializing directories.
   */
  nsresult PopulateDirectoriesImpl(const Directory aRequestedDir);

  /**
   * Resolve the internal PopulateDirectoriesPromise corresponding to
   * |aRequestedDir| with the given result.
   *
   * This will allow all pending queries for the requested directory to resolve
   * or be rejected.
   *
   * @param aRv The return value from PopulateDirectoriesImpl.
   * @param aRequestedDir The requested directory cache entry. This is used to
   *                      determine which internal MozPromiseHolder we are
   * resolving.
   */
  void ResolvePopulateDirectoriesPromise(nsresult aRv,
                                         const Directory aRequestedDir);

  /**
   * Resolve the given JS promise with the path of the requested directory
   *
   * Can only be called once the cache entry for the requested directory is
   * populated.
   *
   * @param aPromise The JS promise to resolve.
   * @param aRequestedDir The requested directory cache entry.
   */
  void ResolveWithDirectory(Promise* aPromise, const Directory aRequestedDir);

  template <typename T>
  using DirectoryArray = EnumeratedArray<Directory, Directory::Count, T>;

  DirectoryArray<nsString> mDirectories;
  DirectoryArray<MozPromiseHolder<PopulateDirectoriesPromise>> mPromises;

  static constexpr DirectoryArray<const char*> kDirectoryNames{
      NS_APP_USER_PROFILE_50_DIR,      NS_APP_USER_PROFILE_LOCAL_50_DIR,
      NS_APP_CONTENT_PROCESS_TEMP_DIR, NS_OS_TEMP_DIR,
      NS_XPCOM_LIBRARY_FILE,
  };
};

}  // namespace dom
}  // namespace mozilla

#endif
