/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IOUtils__
#define mozilla_dom_IOUtils__

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/DataMutex.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/IOUtilsBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/MozPromise.h"
#include "nspr/prio.h"
#include "nsIAsyncShutdown.h"
#include "nsISerialEventTarget.h"
#include "nsLocalFile.h"

namespace mozilla {

/**
 * Utility class to be used with |UniquePtr| to automatically close NSPR file
 * descriptors when they go out of scope.
 *
 * Example:
 *
 *   UniquePtr<PRFileDesc, PR_CloseDelete> fd = PR_Open(path, flags, mode);
 */
class PR_CloseDelete {
 public:
  constexpr PR_CloseDelete() = default;
  PR_CloseDelete(const PR_CloseDelete& aOther) = default;
  PR_CloseDelete(PR_CloseDelete&& aOther) = default;
  PR_CloseDelete& operator=(const PR_CloseDelete& aOther) = default;
  PR_CloseDelete& operator=(PR_CloseDelete&& aOther) = default;

  void operator()(PRFileDesc* aPtr) const { PR_Close(aPtr); }
};

namespace dom {

class IOUtils final {
 public:
  static already_AddRefed<Promise> Read(GlobalObject& aGlobal,
                                        const nsAString& aPath,
                                        const Optional<uint32_t>& aMaxBytes);

  static already_AddRefed<Promise> WriteAtomic(
      GlobalObject& aGlobal, const nsAString& aPath, const Uint8Array& aData,
      const WriteAtomicOptions& aOptions);

  static already_AddRefed<Promise> Move(GlobalObject& aGlobal,
                                        const nsAString& aSourcePath,
                                        const nsAString& aDestPath,
                                        const MoveOptions& aOptions);

  static already_AddRefed<Promise> Remove(GlobalObject& aGlobal,
                                          const nsAString& aPath,
                                          const RemoveOptions& aOptions);

  static bool IsAbsolutePath(const nsAString& aPath);

 private:
  ~IOUtils() = default;

  friend class IOUtilsShutdownBlocker;

  static StaticDataMutex<StaticRefPtr<nsISerialEventTarget>>
      sBackgroundEventTarget;
  static StaticRefPtr<nsIAsyncShutdownClient> sBarrier;
  static Atomic<bool> sShutdownStarted;

  static already_AddRefed<nsIAsyncShutdownClient> GetShutdownBarrier();

  static already_AddRefed<nsISerialEventTarget> GetBackgroundEventTarget();

  static void SetShutdownHooks();

  static already_AddRefed<Promise> CreateJSPromise(GlobalObject& aGlobal);

  /**
   * Opens an existing file at |path|.
   *
   * @param path  The location of the file as an absolute path string.
   * @param flags PRIO flags, excluding |PR_CREATE| and |PR_EXCL|.
   */
  static UniquePtr<PRFileDesc, PR_CloseDelete> OpenExistingSync(
      const nsAString& aPath, int32_t aFlags);

  /**
   * Creates a new file at |path|.
   *
   * @param aPath  The location of the file as an absolute path string.
   * @param aFlags PRIO flags to be used in addition to |PR_CREATE| and
   *               |PR_EXCL|.
   * @param aMode  Optional file mode. Defaults to 0666 to allow the system
   *               umask to compute the best mode for the new file.
   */
  static UniquePtr<PRFileDesc, PR_CloseDelete> CreateFileSync(
      const nsAString& aPath, int32_t aFlags, int32_t aMode = 0666);

  static nsresult ReadSync(PRFileDesc* aFd, const uint32_t aBufSize,
                           nsTArray<uint8_t>& aResult);

  static nsresult WriteSync(PRFileDesc* aFd, const nsTArray<uint8_t>& aBytes,
                            uint32_t& aResult);

  static nsresult MoveSync(const nsAString& aSource, const nsAString& aDest,
                           bool noOverwrite);

  static nsresult RemoveSync(const nsAString& aPath, bool aIgnoreAbsent,
                             bool aRecursive);

  using IOReadMozPromise =
      mozilla::MozPromise<nsTArray<uint8_t>, const nsCString,
                          /* IsExclusive */ true>;

  using IOWriteMozPromise =
      mozilla::MozPromise<uint32_t, const nsCString, /* IsExclusive */ true>;

  using IOMoveMozPromise =
      mozilla::MozPromise<bool /* ignored */, const nsresult,
                          /* IsExclusive */ true>;

  using IORemoveMozPromise =
      mozilla::MozPromise<bool /* ignored */, const nsresult,
                          /* IsExclusive */ true>;
};

class IOUtilsShutdownBlocker : public nsIAsyncShutdownBlocker {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER

 private:
  virtual ~IOUtilsShutdownBlocker() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif
