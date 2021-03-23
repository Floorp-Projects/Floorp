/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IOUtils__
#define mozilla_dom_IOUtils__

#include "js/Utility.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Attributes.h"
#include "mozilla/Buffer.h"
#include "mozilla/DataMutex.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Result.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/IOUtilsBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "nsIAsyncShutdown.h"
#include "nsISerialEventTarget.h"
#include "nsPrintfCString.h"
#include "nsProxyRelease.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
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

/**
 * Implementation for the Web IDL interface at dom/chrome-webidl/IOUtils.webidl.
 * Methods of this class must only be called from the parent process.
 */
class IOUtils final {
 public:
  class IOError;

  static already_AddRefed<Promise> Read(GlobalObject& aGlobal,
                                        const nsAString& aPath,
                                        const ReadOptions& aOptions);

  static already_AddRefed<Promise> ReadUTF8(GlobalObject& aGlobal,
                                            const nsAString& aPath,
                                            const ReadUTF8Options& aOptions);

  static already_AddRefed<Promise> ReadJSON(GlobalObject& aGlobal,
                                            const nsAString& aPath,
                                            const ReadUTF8Options& aOptions);

  static already_AddRefed<Promise> Write(GlobalObject& aGlobal,
                                         const nsAString& aPath,
                                         const Uint8Array& aData,
                                         const WriteOptions& aOptions);

  static already_AddRefed<Promise> WriteUTF8(GlobalObject& aGlobal,
                                             const nsAString& aPath,
                                             const nsACString& aString,
                                             const WriteOptions& aOptions);

  static already_AddRefed<Promise> WriteJSON(GlobalObject& aGlobal,
                                             const nsAString& aPath,
                                             JS::Handle<JS::Value> aValue,
                                             const WriteOptions& aOptions);

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

  static already_AddRefed<Promise> Touch(
      GlobalObject& aGlobal, const nsAString& aPath,
      const Optional<int64_t>& aModification);

  static already_AddRefed<Promise> GetChildren(GlobalObject& aGlobal,
                                               const nsAString& aPath);

  static already_AddRefed<Promise> SetPermissions(GlobalObject& aGlobal,
                                                  const nsAString& aPath,
                                                  uint32_t aPermissions,
                                                  const bool aHonorUmask);

  static already_AddRefed<Promise> Exists(GlobalObject& aGlobal,
                                          const nsAString& aPath);

  static void GetProfileBeforeChange(GlobalObject& aGlobal,
                                     JS::MutableHandle<JS::Value>,
                                     ErrorResult& aRv);

  class JsBuffer;

  /**
   * The kind of buffer to allocate.
   *
   * This controls what kind of JS object (a JSString or a Uint8Array) is
   * returned by |ToJSValue()|.
   */
  enum class BufferKind {
    String,
    Uint8Array,
  };

 private:
  ~IOUtils() = default;

  template <typename T>
  using IOPromise = MozPromise<T, IOError, true>;

  friend class IOUtilsShutdownBlocker;
  struct InternalFileInfo;
  struct InternalWriteOpts;
  class MozLZ4;
  class EventQueue;
  class State;

  /**
   * Dispatch a task on the event queue and resolve or reject the associated
   * promise based on the result.
   *
   * @param aPromise The promise corresponding to the task running on the event
   * queue.
   * @param aFunc The task to run.
   */
  template <typename OkT, typename Fn>
  static void DispatchAndResolve(EventQueue* aQueue, Promise* aPromise,
                                 Fn aFunc);

  /**
   * Creates a new JS Promise.
   *
   * @return The new promise, or |nullptr| on failure.
   */
  static already_AddRefed<Promise> CreateJSPromise(GlobalObject& aGlobal);

  // Allow conversion of |InternalFileInfo| with |ToJSValue|.
  friend bool ToJSValue(JSContext* aCx,
                        const InternalFileInfo& aInternalFileInfo,
                        JS::MutableHandle<JS::Value> aValue);

  /**
   * Attempts to read the entire file at |aPath| into a buffer.
   *
   * @param aFile       The location of the file.
   * @param aMaxBytes   If |Some|, then only read up this this number of bytes,
   *                    otherwise attempt to read the whole file.
   * @param aDecompress If true, decompress the bytes read from disk before
   *                    returning the result to the caller.
   * @param aBufferKind The kind of buffer to allocate.
   *
   * @return A buffer containing the entire (decompressed) file contents, or an
   *         error.
   */
  static Result<JsBuffer, IOError> ReadSync(nsIFile* aFile,
                                            const Maybe<uint32_t>& aMaxBytes,
                                            const bool aDecompress,
                                            BufferKind aBufferKind);

  /*
   * Attempts to read the entire file at |aPath| as a UTF-8 string.
   *
   * @param aFile       The location of the file.
   * @param aDecompress If true, decompress the bytes read from disk before
   *                    returning the result to the caller.
   *
   * @return The (decompressed) contents of the file re-encoded as a UTF-16
   *         string.
   */
  static Result<JsBuffer, IOError> ReadUTF8Sync(nsIFile* aFile,
                                                const bool aDecompress);

  /**
   * Attempt to write the entirety of |aByteArray| to the file at |aPath|.
   * This may occur by writing to an intermediate destination and performing a
   * move, depending on |aOptions|.
   *
   * @param aFile  The location of the file.
   * @param aByteArray The data to write to the file.
   * @param aOptions   Options to modify the way the write is completed.
   *
   * @return The number of bytes written to the file, or an error if the write
   *         failed or was incomplete.
   */
  static Result<uint32_t, IOError> WriteSync(
      nsIFile* aFile, const Span<const uint8_t>& aByteArray,
      const InternalWriteOpts& aOptions);

  /**
   * Attempts to move the file located at |aSourceFile| to |aDestFile|.
   *
   * @param aSourceFile  The location of the file to move.
   * @param aDestFile    The destination for the file.
   * @param noOverWrite If true, abort with an error if a file already exists at
   *                    |aDestFile|. Otherwise, the file will be overwritten by
   *                    the move.
   *
   * @return Ok if the file was moved successfully, or an error.
   */
  static Result<Ok, IOError> MoveSync(nsIFile* aSourceFile, nsIFile* aDestFile,
                                      bool aNoOverwrite);

  /**
   * Attempts to copy the file at |aSourceFile| to |aDestFile|.
   *
   * @param aSourceFile The location of the file to copy.
   * @param aDestFile   The destination that the file will be copied to.
   *
   * @return Ok if the operation was successful, or an error.
   */
  static Result<Ok, IOError> CopySync(nsIFile* aSourceFile, nsIFile* aDestFile,
                                      bool aNoOverWrite, bool aRecursive);

  /**
   * Provides the implementation for |CopySync| and |MoveSync|.
   *
   * @param aMethod      A pointer to one of |nsIFile::MoveTo| or |CopyTo|
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
                                            nsIFile* aSource, nsIFile* aDest,
                                            bool aNoOverwrite);

  /**
   * Attempts to remove the file located at |aFile|.
   *
   * @param aFile         The location of the file.
   * @param aIgnoreAbsent If true, suppress errors due to an absent target file.
   * @param aRecursive    If true, attempt to recursively remove descendant
   *                      files. This option is safe to use even if the target
   *                      is not a directory.
   *
   * @return Ok if the file was removed successfully, or an error.
   */
  static Result<Ok, IOError> RemoveSync(nsIFile* aFile, bool aIgnoreAbsent,
                                        bool aRecursive);

  /**
   * Attempts to create a new directory at |aFile|.
   *
   * @param aFile             The location of the directory to create.
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
  static Result<Ok, IOError> MakeDirectorySync(nsIFile* aFile,
                                               bool aCreateAncestors,
                                               bool aIgnoreExisting,
                                               int32_t aMode = 0777);

  /**
   * Attempts to stat a file at |aFile|.
   *
   * @param aFile The location of the file.
   *
   * @return An |InternalFileInfo| struct if successful, or an error.
   */
  static Result<IOUtils::InternalFileInfo, IOError> StatSync(nsIFile* aFile);

  /**
   * Attempts to update the last modification time of the file at |aFile|.
   *
   * @param aFile       The location of the file.
   * @param aNewModTime Some value in milliseconds since Epoch. For the current
   *                    system time, use |Nothing|.
   *
   * @return Timestamp of the file if the operation was successful, or an error.
   */
  static Result<int64_t, IOError> TouchSync(nsIFile* aFile,
                                            const Maybe<int64_t>& aNewModTime);

  /**
   * Returns the immediate children of the directory at |aFile|, if any.
   *
   * @param aFile The location of the directory.
   *
   * @return An array of absolute paths identifying the children of |aFile|.
   *         If there are no children, an empty array. Otherwise, an error.
   */
  static Result<nsTArray<nsString>, IOError> GetChildrenSync(nsIFile* aFile);

  /**
   * Set the permissions of the given file.
   *
   * Windows does not make a distinction between user, group, and other
   * permissions like UNICES do. If a permission flag is set for any of user,
   * group, or other has a permission, then all users will have that
   * permission.
   *
   * @param aFile        The location of the file.
   * @param aPermissions The permissions to set, as a UNIX file mode.
   *
   * @return |Ok| if the permissions were successfully set, or an error.
   */
  static Result<Ok, IOError> SetPermissionsSync(nsIFile* aFile,
                                                const uint32_t aPermissions);

  /**
   * Return whether or not the file exists.
   *
   * @param aFile The location of the file.
   *
   * @return Whether or not the file exists.
   */
  static Result<bool, IOError> ExistsSync(nsIFile* aFile);

  enum class EventQueueStatus {
    Uninitialized,
    Initialized,
    Shutdown,
  };

  enum class ShutdownBlockerStatus {
    Uninitialized,
    Initialized,
    Failed,
  };

  /**
   * Internal IOUtils state.
   */
  class State {
   public:
    StaticAutoPtr<EventQueue> mEventQueue;
    EventQueueStatus mQueueStatus = EventQueueStatus::Uninitialized;
    ShutdownBlockerStatus mBlockerStatus = ShutdownBlockerStatus::Uninitialized;

    /**
     * Set up shutdown hooks to free our internals at shutdown.
     *
     * NB: Must be called on main thread.
     */
    void SetShutdownHooks();
  };

  using StateMutex = StaticDataMutex<State>;

  /**
   * Lock the state mutex and return a handle. If shutdown has not yet
   * finished, the internals will be constructed if necessary.
   *
   * @returns A handle to the internal state, which can be used to retrieve the
   *          event queue.
   *          If |Some| is returned, |mEventQueue| is guaranteed to be
   * initialized. If shutdown has finished, |Nothing| is returned.
   */
  static Maybe<StateMutex::AutoLock> GetState();

  static StateMutex sState;
};

/**
 * The IOUtils event queue.
 */
class IOUtils::EventQueue final {
  friend void IOUtils::State::SetShutdownHooks();

 public:
  EventQueue();

  EventQueue(const EventQueue&) = delete;
  EventQueue(EventQueue&&) = delete;
  EventQueue& operator=(const EventQueue&) = delete;
  EventQueue& operator=(EventQueue&&) = delete;

  template <typename OkT, typename Fn>
  RefPtr<IOPromise<OkT>> Dispatch(Fn aFunc);

  Result<already_AddRefed<nsIAsyncShutdownClient>, nsresult>
  GetProfileBeforeChangeClient();

  Result<already_AddRefed<nsIAsyncShutdownBarrier>, nsresult>
  GetProfileBeforeChangeBarrier();

 private:
  nsresult SetShutdownHooks();

  nsCOMPtr<nsISerialEventTarget> mBackgroundEventTarget;
  nsCOMPtr<nsIAsyncShutdownBarrier> mProfileBeforeChangeBarrier;
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
  IOError WithMessage(const nsCString& aMessage) {
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
  FileType mType = FileType::Other;
  uint64_t mSize = 0;
  uint64_t mLastModified = 0;
  Maybe<uint64_t> mCreationTime;
  uint32_t mPermissions = 0;
};

/**
 * This is an easier to work with representation of a
 * |mozilla::dom::WriteOptions| for private use in the |IOUtils|
 * implementation.
 *
 * Because web IDL dictionaries are not easily copy/moveable, this class is
 * used instead.
 */
struct IOUtils::InternalWriteOpts {
  RefPtr<nsIFile> mBackupFile;
  RefPtr<nsIFile> mTmpFile;
  bool mFlush = false;
  bool mNoOverwrite = false;
  bool mCompress = false;

  static Result<InternalWriteOpts, IOUtils::IOError> FromBinding(
      const WriteOptions& aOptions);
};

/**
 * Re-implements the file compression and decompression utilities found
 * in toolkit/components/lz4/lz4.js
 *
 * This implementation uses the non-standard data layout:
 *
 *  - MAGIC_NUMBER (8 bytes)
 *  - content size (uint32_t, little endian)
 *  - content, as obtained from mozilla::Compression::LZ4::compress
 *
 * See bug 1209390 for more info.
 */
class IOUtils::MozLZ4 {
 public:
  static constexpr std::array<uint8_t, 8> MAGIC_NUMBER{
      {'m', 'o', 'z', 'L', 'z', '4', '0', '\0'}};

  static const uint32_t HEADER_SIZE = 8 + sizeof(uint32_t);

  /**
   * Compresses |aUncompressed| byte array, and returns a byte array with the
   * correct format whose contents may be written to disk.
   */
  static Result<nsTArray<uint8_t>, IOError> Compress(
      Span<const uint8_t> aUncompressed);

  /**
   * Checks |aFileContents| for the correct file header, and returns the
   * decompressed content.
   */
  static Result<IOUtils::JsBuffer, IOError> Decompress(
      Span<const uint8_t> aFileContents, IOUtils::BufferKind);
};

class IOUtilsShutdownBlocker : public nsIAsyncShutdownBlocker,
                               public nsIAsyncShutdownCompletionCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER
  NS_DECL_NSIASYNCSHUTDOWNCOMPLETIONCALLBACK

  enum Phase {
    ProfileBeforeChange,
    XpcomWillShutdown,
  };

  explicit IOUtilsShutdownBlocker(Phase aPhase) : mPhase(aPhase) {}

 private:
  virtual ~IOUtilsShutdownBlocker() = default;

  Phase mPhase;
  RefPtr<nsIAsyncShutdownClient> mParentClient;
};

/**
 * A buffer that is allocated inside one of JS heaps so that it can be converted
 * to a JSString or Uint8Array object with at most one copy in the worst case.
 */
class IOUtils::JsBuffer final {
 public:
  /**
   * Create a new buffer of the given kind with the requested capacity.
   *
   * @param aBufferKind The kind of buffer to create (either a string or an
   *                    array).
   * @param aCapacity The capacity of the buffer.
   *
   * @return Either a successfully created buffer or an error if it could not be
   * allocated.
   */
  static Result<JsBuffer, IOUtils::IOError> Create(
      IOUtils::BufferKind aBufferKind, size_t aCapacity);

  /**
   * Create a new, empty buffer.
   *
   * This operation cannot fail.
   *
   * @param aBufferKind The kind of buffer to create (either a string or an
   *                    array).
   *
   * @return An empty JsBuffer.
   */
  static JsBuffer CreateEmpty(IOUtils::BufferKind aBufferKind);

  JsBuffer(const JsBuffer&) = delete;
  JsBuffer(JsBuffer&& aOther) noexcept;
  JsBuffer& operator=(const JsBuffer&) = delete;
  JsBuffer& operator=(JsBuffer&& aOther) noexcept;

  size_t Length() { return mLength; }
  char* Elements() { return mBuffer.get(); }
  void SetLength(size_t aNewLength) {
    MOZ_RELEASE_ASSERT(aNewLength <= mCapacity);
    mLength = aNewLength;
  }

  /**
   * Return a span for writing to the buffer.
   *
   * |SetLength| should be called after the buffer has been written to.
   *
   * @returns A span for writing to. The size of the span is the entire
   *          allocated capacity.
   */
  Span<char> BeginWriting() {
    MOZ_RELEASE_ASSERT(mBuffer.get());
    return Span(mBuffer.get(), mCapacity);
  }

  /**
   * Return a span for reading from.
   *
   * @returns A span for reading form. The size of the span is the set length
   *          of the buffer.
   */
  Span<const char> BeginReading() const {
    MOZ_RELEASE_ASSERT(mBuffer.get() || mLength == 0);
    return Span(mBuffer.get(), mLength);
  }

  /**
   * Consume the JsBuffer and convert it into a JSString.
   *
   * NOTE: This method asserts the buffer was allocated as a string buffer.
   *
   * @param aBuffer The buffer to convert to a string. After this call, the
   *                buffer will be invaldated and |IntoString| cannot be called
   *                again.
   *
   * @returns A JSString with the contents of |aBuffer|.
   */
  static JSString* IntoString(JSContext* aCx, JsBuffer aBuffer);

  /**
   * Consume the JsBuffer and convert it into a Uint8Array.
   *
   * NOTE: This method asserts the buffer was allocated as an array buffer.
   *
   * @param aBuffer The buffer to convert to an array. After this call, the
   *                buffer will be invalidated and |IntoUint8Array| cannot be
   *                called again.
   *
   * @returns A JSBuffer
   */
  static JSObject* IntoUint8Array(JSContext* aCx, JsBuffer aBuffer);

  friend bool ToJSValue(JSContext* aCx, JsBuffer&& aBuffer,
                        JS::MutableHandle<JS::Value> aValue);

 private:
  IOUtils::BufferKind mBufferKind;
  size_t mCapacity;
  size_t mLength;
  JS::UniqueChars mBuffer;

  JsBuffer(BufferKind aBufferKind, size_t aCapacity);
};

}  // namespace dom
}  // namespace mozilla

#endif
