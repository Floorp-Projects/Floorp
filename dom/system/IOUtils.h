/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IOUtils__
#define mozilla_dom_IOUtils__

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Buffer.h"
#include "mozilla/DataMutex.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/IOUtilsBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Result.h"
#include "nsStringFwd.h"
#include "nsIAsyncShutdown.h"
#include "nsISerialEventTarget.h"
#include "nsLocalFile.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "prio.h"

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
  class IOError;

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

  static already_AddRefed<Promise> MakeDirectory(
      GlobalObject& aGlobal, const nsAString& aPath,
      const MakeDirectoryOptions& aOptions);

  static already_AddRefed<Promise> Stat(GlobalObject& aGlobal,
                                        const nsAString& aPath);

  static already_AddRefed<Promise> Copy(GlobalObject& aGlobal,
                                        const nsAString& aSourcePath,
                                        const nsAString& aDestPath,
                                        const CopyOptions& aOptions);

  static bool IsAbsolutePath(const nsAString& aPath);

 private:
  ~IOUtils() = default;

  friend class IOUtilsShutdownBlocker;
  struct InternalFileInfo;
  struct InternalWriteAtomicOpts;

  static StaticDataMutex<StaticRefPtr<nsISerialEventTarget>>
      sBackgroundEventTarget;
  static StaticRefPtr<nsIAsyncShutdownClient> sBarrier;
  static Atomic<bool> sShutdownStarted;

  static already_AddRefed<nsIAsyncShutdownClient> GetShutdownBarrier();

  static already_AddRefed<nsISerialEventTarget> GetBackgroundEventTarget();

  static void SetShutdownHooks();

  template <typename OkT, typename Fn, typename... Args>
  static already_AddRefed<Promise> RunOnBackgroundThread(
      RefPtr<Promise>& aPromise, Fn aFunc, Args... aArgs);

  /**
   * Creates a new JS Promise.
   *
   * @return The new promise, or |nullptr| on failure.
   */
  static already_AddRefed<Promise> CreateJSPromise(GlobalObject& aGlobal);

  // Allow conversion of |InternalFileInfo| with |ToJSValue|.
  friend MOZ_MUST_USE bool ToJSValue(JSContext* aCx,
                                     const InternalFileInfo& aInternalFileInfo,
                                     JS::MutableHandle<JS::Value> aValue);

  /**
   * Rejects |aPromise| with an appropriate |DOMException| describing |aError|.
   */
  static void RejectJSPromise(const RefPtr<Promise>& aPromise,
                              const IOError& aError);

  /**
   * Opens an existing file at |aPath|.
   *
   * @param path  The location of the file as an absolute path string.
   * @param flags PRIO flags, excluding |PR_CREATE| and |PR_EXCL|.
   *
   * @return A pointer to the opened file descriptor, or |nullptr| on failure.
   */
  static UniquePtr<PRFileDesc, PR_CloseDelete> OpenExistingSync(
      const nsAString& aPath, int32_t aFlags);

  /**
   * Creates a new file at |aPath|.
   *
   * @param aPath  The location of the file as an absolute path string.
   * @param aFlags PRIO flags to be used in addition to |PR_CREATE| and
   *               |PR_EXCL|.
   * @param aMode  Optional file mode. Defaults to 0666 to allow the system
   *               umask to compute the best mode for the new file.
   *
   * @return A pointer to the opened file descriptor, or |nullptr| on failure.
   */
  static UniquePtr<PRFileDesc, PR_CloseDelete> CreateFileSync(
      const nsAString& aPath, int32_t aFlags, int32_t aMode = 0666);

  /**
   * Attempts to read the entire file at |aPath| into a buffer.
   *
   * @param aPath     The location of the file as an absolute path string.
   * @param aMaxBytes If |Some|, then only read up this this number of bytes,
   *                  otherwise attempt to read the whole file.
   *
   * @return A byte array of the entire file contents, or an error.
   */
  static Result<nsTArray<uint8_t>, IOError> ReadSync(
      const nsAString& aPath, const Maybe<uint32_t>& aMaxBytes);

  /**
   * Attempts to write the entirety of |aByteArray| to the file at |aPath|.
   * This may occur by writing to an intermediate destination and performing a
   * move, depending on |aOptions|.
   *
   * @param aDestPath  The location of the file as an absolute path string.
   * @param aByteArray The data to write to the file.
   * @param aOptions   Options to modify the way the write is completed.
   *
   * @return The number of bytes written to the file, or an error if the write
   *         failed or was incomplete.
   */
  static Result<uint32_t, IOError> WriteAtomicSync(
      const nsAString& aDestPath, const Buffer<uint8_t>& aByteArray,
      const InternalWriteAtomicOpts& aOptions);

  /**
   * Attempts to write |aBytes| to the file pointed by |aFd|.
   *
   * @param aFd    An open PRFileDesc for the destination file to be
   *               overwritten.
   * @param aBytes The data to write to the file.
   *
   * @return The number of bytes written to the file, or an error if the write
   *         failed or was incomplete.
   */
  static Result<uint32_t, IOError> WriteSync(PRFileDesc* aFd,
                                             const nsACString& aPath,
                                             const Buffer<uint8_t>& aBytes);

  /**
   * Attempts to move the file located at |aSource| to |aDest|.
   *
   * @param aSource     The location of the file to move as an absolute path
   *                    string.
   * @param aDest       The destination for the file as an absolute path string.
   * @param noOverWrite If true, abort with an error if a file already exists at
   *                    |aDest|. Otherwise, the file will be overwritten by the
   *                    move.
   *
   * @return Ok if the file was moved successfully, or an error.
   */
  static Result<Ok, IOError> MoveSync(const nsAString& aSourcePath,
                                      const nsAString& aDestPath,
                                      bool aNoOverwrite);

  /**
   * Attempts to copy the file at |aSourcePath| to |aDestPath|.
   *
   * @param aSourcePath The location of the file to be copied as an absolute
   *                    path string.
   * @param aDestPath   The location that the file should be copied to, as an
   *                    absolute path string.
   *
   * @return Ok if the operation was successful, or an error.
   */
  static Result<Ok, IOError> CopySync(const nsAString& aSourcePath,
                                      const nsAString& aDestPath,
                                      bool aNoOverWrite, bool aRecursive);

  /**
   * Provides the implementation for |CopySync| and |MoveSync|.
   *
   * @param aMethod      A pointer to one of |nsLocalFile::MoveTo| or |CopyTo|
   *                     instance methods.
   * @param aMethodName  The name of the method to the performed. Either "move"
   *                     or "copy".
   * @param aSource      The source file to be copied or moved.
   * @param aDest        The destination file.
   * @param aNoOverwrite If true, allow overwriting |aDest| during the copy or
   *                     move. Otherwise, abort with an error if the file would
   *                     be overwritten.
   *
   * @return Ok if the operation was successful, or an error.
   */
  template <typename CopyOrMoveFn>
  static Result<Ok, IOError> CopyOrMoveSync(CopyOrMoveFn aMethod,
                                            const char* aMethodName,
                                            const RefPtr<nsLocalFile>& aSource,
                                            const RefPtr<nsLocalFile>& aDest,
                                            bool aNoOverwrite);

  /**
   * Attempts to remove the file located at |aPath|.
   *
   * @param aPath         The location of the file as an absolute path string.
   * @param aIgnoreAbsent If true, suppress errors due to an absent target file.
   * @param aRecursive    If true, attempt to recursively remove descendant
   *                      files. This option is safe to use even if the target
   *                      is not a directory.
   *
   * @return Ok if the file was removed successfully, or an error.
   */
  static Result<Ok, IOError> RemoveSync(const nsAString& aPath,
                                        bool aIgnoreAbsent, bool aRecursive);

  /**
   * Attempts to create a new directory at |aPath|.
   *
   * @param aPath             The location of the file as an absolute path
   *                          string.
   * @param aCreateAncestors  If true, create missing ancestor directories as
   *                          needed. Otherwise, report an error if the target
   *                          has non-existing ancestor directories.
   * @param aIgnoreExisting   If true, suppress errors that occur if the target
   *                          directory already exists. Otherwise, propagate the
   *                          error if it occurs.
   * @param aMode             Optional file mode. Defaults to 0777 to allow the
   *                          system umask to compute the best mode for the new
   *                          directory.
   *
   * @return Ok if the directory was created successfully, or an error.
   */
  static Result<Ok, IOError> CreateDirectorySync(const nsAString& aPath,
                                                 bool aCreateAncestors,
                                                 bool aIgnoreExisting,
                                                 int32_t aMode = 0777);

  /**
   * Attempts to stat a file at |aPath|.
   *
   * @param aPath The location of the file as an absolute path string.
   *
   * @return An |InternalFileInfo| struct if successful, or an error.
   */
  static Result<IOUtils::InternalFileInfo, IOError> StatSync(
      const nsAString& aPath);
};

/**
 * An error class used with the |Result| type returned by most private |IOUtils|
 * methods.
 */
class IOUtils::IOError {
 public:
  MOZ_IMPLICIT IOError(nsresult aCode) : mCode(aCode), mMessage(Nothing()) {}

  /**
   * Replaces the message associated with this error.
   */
  template <typename... Args>
  IOError WithMessage(const char* const aMessage, Args... aArgs) {
    mMessage.emplace(nsPrintfCString(aMessage, aArgs...));
    return *this;
  }
  IOError WithMessage(const char* const aMessage) {
    mMessage.emplace(nsCString(aMessage));
    return *this;
  }
  IOError WithMessage(nsCString aMessage) {
    mMessage.emplace(aMessage);
    return *this;
  }

  /**
   * Returns the |nsresult| associated with this error.
   */
  nsresult Code() const { return mCode; }

  /**
   * Maybe returns a message associated with this error.
   */
  const Maybe<nsCString>& Message() const { return mMessage; }

 private:
  nsresult mCode;
  Maybe<nsCString> mMessage;
};

/**
 * This is an easier to work with representation of a |mozilla::dom::FileInfo|
 * for private use in the IOUtils implementation.
 *
 * Because web IDL dictionaries are not easily copy/moveable, this class is
 * used instead, until converted to the proper |mozilla::dom::FileInfo| before
 * returning any results to JavaScript.
 */
struct IOUtils::InternalFileInfo {
  nsString mPath;
  FileType mType;
  uint64_t mSize;
  uint64_t mLastModified;
};

/**
 * This is an easier to work with representation of a
 * |mozilla::dom::WriteAtomicOptions| for private use in the |IOUtils|
 * implementation.
 *
 * Because web IDL dictionaries are not easily copy/moveable, this class is
 * used instead.
 */
struct IOUtils::InternalWriteAtomicOpts {
  Maybe<nsString> mBackupFile;
  bool mFlush;
  bool mNoOverwrite;
  Maybe<nsString> mTmpPath;
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
