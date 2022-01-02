/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScriptPreloader_h
#define ScriptPreloader_h

#include "mozilla/Atomics.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EnumSet.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Maybe.h"
#include "mozilla/MaybeOneOf.h"
#include "mozilla/Monitor.h"
#include "mozilla/Range.h"
#include "mozilla/Vector.h"
#include "mozilla/Result.h"
#include "mozilla/loader/AutoMemMap.h"
#include "nsClassHashtable.h"
#include "nsIAsyncShutdown.h"
#include "nsIFile.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsIThread.h"
#include "nsITimer.h"

#include "js/CompileOptions.h"  // JS::DecodeOptions
#include "js/experimental/JSStencil.h"
#include "js/GCAnnotations.h"  // for JS_HAZ_NON_GC_POINTER
#include "js/RootingAPI.h"     // for Handle, Heap
#include "js/Transcoding.h"  // for TranscodeBuffer, TranscodeRange, TranscodeSources
#include "js/TypeDecls.h"  // for HandleObject, HandleScript

#include <prio.h>

namespace JS {
class CompileOptions;
class OffThreadToken;
}  // namespace JS

namespace mozilla {
namespace dom {
class ContentParent;
}
namespace ipc {
class FileDescriptor;
}
namespace loader {
class InputBuffer;
class ScriptCacheChild;

enum class ProcessType : uint8_t {
  Uninitialized,
  Parent,
  Web,
  Extension,
  PrivilegedAbout,
};

template <typename T>
struct Matcher {
  virtual bool Matches(T) = 0;
};
}  // namespace loader

using namespace mozilla::loader;

class ScriptPreloader : public nsIObserver,
                        public nsIMemoryReporter,
                        public nsIRunnable,
                        public nsINamed,
                        public nsIAsyncShutdownBlocker {
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

  friend class mozilla::loader::ScriptCacheChild;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEMORYREPORTER
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSINAMED
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER

 private:
  static StaticRefPtr<ScriptPreloader> gScriptPreloader;
  static StaticRefPtr<ScriptPreloader> gChildScriptPreloader;
  static UniquePtr<AutoMemMap> gCacheData;
  static UniquePtr<AutoMemMap> gChildCacheData;

 public:
  static ScriptPreloader& GetSingleton();
  static ScriptPreloader& GetChildSingleton();

  static void DeleteSingleton();
  static void DeleteCacheDataSingleton();

  static ProcessType GetChildProcessType(const nsACString& remoteType);

  // Fill some options that should be consistent across all scripts stored
  // into preloader cache.
  static void FillCompileOptionsForCachedStencil(JS::CompileOptions& options);
  static void FillDecodeOptionsForCachedStencil(JS::DecodeOptions& options);

  // Retrieves the stencil with the given cache key from the cache.
  // Returns null if the stencil is not cached.
  already_AddRefed<JS::Stencil> GetCachedStencil(
      JSContext* cx, const JS::DecodeOptions& options, const nsCString& path);

  // Notes the execution of a script with the given URL and cache key.
  // Depending on the stage of startup, the script may be serialized and
  // stored to the startup script cache.
  //
  // If isRunOnce is true, this script is expected to run only once per
  // process per browser session. A cached instance will not be kept alive
  // for repeated execution.
  void NoteStencil(const nsCString& url, const nsCString& cachePath,
                   JS::Stencil* stencil, bool isRunOnce = false);

  // Notes the IPC arrival of the XDR data of a stencil compiled by some
  // child process. See ScriptCacheChild::SendScriptsAndFinalize.
  void NoteStencil(const nsCString& url, const nsCString& cachePath,
                   ProcessType processType, nsTArray<uint8_t>&& xdrData,
                   TimeStamp loadTime);

  // Initializes the script cache from the startup script cache file.
  Result<Ok, nsresult> InitCache(const nsAString& = u"scriptCache"_ns);

  Result<Ok, nsresult> InitCache(const Maybe<ipc::FileDescriptor>& cacheFile,
                                 ScriptCacheChild* cacheChild);

  bool Active() const { return mCacheInitialized && !mStartupFinished; }

 private:
  Result<Ok, nsresult> InitCacheInternal(JS::HandleObject scope = nullptr);
  already_AddRefed<JS::Stencil> GetCachedStencilInternal(
      JSContext* cx, const JS::DecodeOptions& options, const nsCString& path);

 public:
  static ProcessType CurrentProcessType() {
    MOZ_ASSERT(sProcessType != ProcessType::Uninitialized);
    return sProcessType;
  }

  static void InitContentChild(dom::ContentParent& parent);

 protected:
  virtual ~ScriptPreloader();

 private:
  enum class ScriptStatus {
    Restored,
    Saved,
  };

  // Represents a cached script stencil, either initially read from the
  // cache file, to be added to the next session's stencil cache file, or
  // both.
  //
  //  - Read from the cache, and being decoded off thread. In this case,
  //    mReadyToExecute is false, and mToken is null.
  //  - Off-thread decode has finished, but the stencil has not yet been
  //    executed. In this case, mReadyToExecute is true, and mToken has a
  //    non-null value.
  //  - Read from the cache, but too small or needed to immediately to be
  //    compiled off-thread. In this case, mReadyToExecute is true, and both
  //    mToken and mStencil are null.
  //  - Fully decoded, and ready to be added to the next session's cache
  //    file. In this case, mReadyToExecute is true, and mStencil is non-null.
  //
  // A stencil to be added to the next session's cache file always has a
  // non-null mStencil value. If it was read from the last session's cache
  // file, it also has a non-empty mXDRRange range, which will be stored in
  // the next session's cache file. If it was compiled in this session, its
  // mXDRRange will initially be empty, and its mXDRData buffer will be
  // populated just before it is written to the cache file.
  class CachedStencil : public LinkedListElement<CachedStencil> {
   public:
    CachedStencil(CachedStencil&&) = delete;

    CachedStencil(ScriptPreloader& cache, const nsCString& url,
                  const nsCString& cachePath, JS::Stencil* stencil)
        : mCache(cache),
          mURL(url),
          mCachePath(cachePath),
          mStencil(stencil),
          mReadyToExecute(true),
          mIsRunOnce(false) {}

    inline CachedStencil(ScriptPreloader& cache, InputBuffer& buf);

    ~CachedStencil() = default;

    ScriptStatus Status() const {
      return mProcessTypes.isEmpty() ? ScriptStatus::Restored
                                     : ScriptStatus::Saved;
    }

    // For use with nsTArray::Sort.
    //
    // Orders scripts by script load time, so that scripts which are needed
    // earlier are stored earlier, and scripts needed at approximately the
    // same time are stored approximately contiguously.
    struct Comparator {
      bool Equals(const CachedStencil* a, const CachedStencil* b) const {
        return a->mLoadTime == b->mLoadTime;
      }

      bool LessThan(const CachedStencil* a, const CachedStencil* b) const {
        return a->mLoadTime < b->mLoadTime;
      }
    };

    struct StatusMatcher final : public Matcher<CachedStencil*> {
      explicit StatusMatcher(ScriptStatus status) : mStatus(status) {}

      virtual bool Matches(CachedStencil* script) override {
        return script->Status() == mStatus;
      }

      const ScriptStatus mStatus;
    };

    void FreeData() {
      // If the script data isn't mmapped, we need to release both it
      // and the Range that points to it at the same time.
      if (!IsMemMapped()) {
        mXDRRange.reset();
        mXDRData.destroy();
      }
    }

    void UpdateLoadTime(const TimeStamp& loadTime) {
      if (mLoadTime.IsNull() || loadTime < mLoadTime) {
        mLoadTime = loadTime;
      }
    }

    // Checks whether the cached JSScript for this entry will be needed
    // again and, if not, drops it and returns true. This is the case for
    // run-once scripts that do not still need to be encoded into the
    // cache.
    //
    // If this method returns false, callers may set mScript to a cached
    // JSScript instance for this entry. If it returns true, they should
    // not.
    bool MaybeDropStencil() {
      if (mIsRunOnce && (HasRange() || !mCache.WillWriteScripts())) {
        mStencil = nullptr;
        return true;
      }
      return false;
    }

    // Encodes this script into XDR data, and stores the result in mXDRData.
    // Returns true on success, false on failure.
    bool XDREncode(JSContext* cx);

    // Encodes or decodes this script, in the storage format required by the
    // script cache file.
    template <typename Buffer>
    void Code(Buffer& buffer) {
      buffer.codeString(mURL);
      buffer.codeString(mCachePath);
      buffer.codeUint32(mOffset);
      buffer.codeUint32(mSize);
      buffer.codeUint8(mProcessTypes);
    }

    // Returns the XDR data generated for this script during this session. See
    // mXDRData.
    JS::TranscodeBuffer& Buffer() {
      MOZ_ASSERT(HasBuffer());
      return mXDRData.ref<JS::TranscodeBuffer>();
    }

    bool HasBuffer() { return mXDRData.constructed<JS::TranscodeBuffer>(); }

    // Returns the read-only XDR data for this script. See mXDRRange.
    const JS::TranscodeRange& Range() {
      MOZ_ASSERT(HasRange());
      return mXDRRange.ref();
    }

    bool HasRange() { return mXDRRange.isSome(); }

    bool IsMemMapped() const { return mXDRData.empty(); }

    nsTArray<uint8_t>& Array() {
      MOZ_ASSERT(HasArray());
      return mXDRData.ref<nsTArray<uint8_t>>();
    }

    bool HasArray() { return mXDRData.constructed<nsTArray<uint8_t>>(); }

    already_AddRefed<JS::Stencil> GetStencil(JSContext* cx,
                                             const JS::DecodeOptions& options);

    size_t HeapSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) {
      auto size = mallocSizeOf(this);

      if (HasArray()) {
        size += Array().ShallowSizeOfExcludingThis(mallocSizeOf);
      } else if (HasBuffer()) {
        size += Buffer().sizeOfExcludingThis(mallocSizeOf);
      }

      if (mStencil) {
        size += JS::SizeOfStencil(mStencil, mallocSizeOf);
      }

      // Note: mURL and mCachePath use the same string for scripts loaded
      // by the message manager. The following statement avoids
      // double-measuring in that case.
      size += (mURL.SizeOfExcludingThisIfUnshared(mallocSizeOf) +
               mCachePath.SizeOfExcludingThisEvenIfShared(mallocSizeOf));

      return size;
    }

    ScriptPreloader& mCache;

    // The URL from which this script was initially read and compiled.
    nsCString mURL;
    // A unique identifier for this script's filesystem location, used as a
    // primary cache lookup value.
    nsCString mCachePath;

    // The offset of this script in the cache file, from the start of the XDR
    // data block.
    uint32_t mOffset = 0;
    // The size of this script's encoded XDR data.
    uint32_t mSize = 0;

    TimeStamp mLoadTime{};

    RefPtr<JS::Stencil> mStencil;

    // True if this script is ready to be executed. This means that either the
    // off-thread portion of an off-thread decode has finished, or the script
    // is too small to be decoded off-thread, and may be immediately decoded
    // whenever it is first executed.
    bool mReadyToExecute = false;

    // True if this script is expected to run once per process. If so, its
    // JSScript instance will be dropped as soon as the script has
    // executed and been encoded into the cache.
    bool mIsRunOnce = false;

    // The set of processes in which this script has been used.
    EnumSet<ProcessType> mProcessTypes{};

    // The set of processes which the script was loaded into during the
    // last session, as read from the cache file.
    EnumSet<ProcessType> mOriginalProcessTypes{};

    // The read-only XDR data for this script, which was either read from an
    // existing cache file, or generated by encoding a script which was
    // compiled during this session.
    Maybe<JS::TranscodeRange> mXDRRange;

    // XDR data which was generated from a script compiled during this
    // session, and will be written to the cache file.
    //
    // The format is JS::TranscodeBuffer if the script was XDR'd as part
    // of this process, or nsTArray<> if the script was transfered by IPC
    // from a child process.
    MaybeOneOf<JS::TranscodeBuffer, nsTArray<uint8_t>> mXDRData;
  } JS_HAZ_NON_GC_POINTER;

  template <ScriptStatus status>
  static Matcher<CachedStencil*>* Match() {
    static CachedStencil::StatusMatcher matcher{status};
    return &matcher;
  }

  // There's a significant setup cost for each off-thread decode operation,
  // so scripts are decoded in chunks to minimize the overhead. There's a
  // careful balancing act in choosing the size of chunks, to minimize the
  // number of decode operations, while also minimizing the number of buffer
  // underruns that require the main thread to wait for a script to finish
  // decoding.
  //
  // For the first chunk, we don't have much time between the start of the
  // decode operation and the time the first script is needed, so that chunk
  // needs to be fairly small. After the first chunk is finished, we have
  // some buffered scripts to fall back on, and a lot more breathing room,
  // so the chunks can be a bit bigger, but still not too big.
  static constexpr int OFF_THREAD_FIRST_CHUNK_SIZE = 128 * 1024;
  static constexpr int OFF_THREAD_CHUNK_SIZE = 512 * 1024;

  // Ideally, we want every chunk to be smaller than the chunk sizes
  // specified above. However, if we have some number of small scripts
  // followed by a huge script that would put us over the normal chunk size,
  // we're better off processing them as a single chunk.
  //
  // In order to guarantee that the JS engine will process a chunk
  // off-thread, it needs to be at least 100K (which is an implementation
  // detail that can change at any time), so make sure that we always hit at
  // least that size, with a bit of breathing room to be safe.
  static constexpr int SMALL_SCRIPT_CHUNK_THRESHOLD = 128 * 1024;

  // The maximum size of scripts to re-decode on the main thread if off-thread
  // decoding hasn't finished yet. In practice, we don't hit this very often,
  // but when we do, re-decoding some smaller scripts on the main thread gives
  // the background decoding a chance to catch up without blocking the main
  // thread for quite as long.
  static constexpr int MAX_MAINTHREAD_DECODE_SIZE = 50 * 1024;

  explicit ScriptPreloader(AutoMemMap* cacheData);

  void Cleanup();

  void FinishPendingParses(MonitorAutoLock& aMal);
  void InvalidateCache();

  // Opens the cache file for reading.
  Result<Ok, nsresult> OpenCache();

  // Writes a new cache file to disk. Must not be called on the main thread.
  Result<Ok, nsresult> WriteCache();

  void StartCacheWrite();

  // Prepares scripts for writing to the cache, serializing new scripts to
  // XDR, and calculating their size-based offsets.
  void PrepareCacheWrite();

  void PrepareCacheWriteInternal();

  void CacheWriteComplete();

  void FinishContentStartup();

  // Returns true if scripts added to the cache now will be encoded and
  // written to the cache. If we've already encoded scripts for the cache
  // write, or this is a content process which hasn't been asked to return
  // script bytecode, this will return false.
  bool WillWriteScripts();

  // Returns a file pointer for the cache file with the given name in the
  // current profile.
  Result<nsCOMPtr<nsIFile>, nsresult> GetCacheFile(const nsAString& suffix);

  // Waits for the given cached script to finish compiling off-thread, or
  // decodes it synchronously on the main thread, as appropriate.
  already_AddRefed<JS::Stencil> WaitForCachedStencil(
      JSContext* cx, const JS::DecodeOptions& options, CachedStencil* script);

  void DecodeNextBatch(size_t chunkSize, JS::HandleObject scope = nullptr);

  static void OffThreadDecodeCallback(JS::OffThreadToken* token, void* context);
  void FinishOffThreadDecode(JS::OffThreadToken* token);
  void DoFinishOffThreadDecode();

  already_AddRefed<nsIAsyncShutdownClient> GetShutdownBarrier();

  size_t ShallowHeapSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) {
    return (mallocSizeOf(this) +
            mScripts.ShallowSizeOfExcludingThis(mallocSizeOf) +
            mallocSizeOf(mSaveThread.get()) + mallocSizeOf(mProfD.get()));
  }

  using ScriptHash = nsClassHashtable<nsCStringHashKey, CachedStencil>;

  template <ScriptStatus status>
  static size_t SizeOfHashEntries(ScriptHash& scripts,
                                  mozilla::MallocSizeOf mallocSizeOf) {
    size_t size = 0;
    for (auto elem : IterHash(scripts, Match<status>())) {
      size += elem->HeapSizeOfIncludingThis(mallocSizeOf);
    }
    return size;
  }

  ScriptHash mScripts;

  // True after we've shown the first window, and are no longer adding new
  // scripts to the cache.
  bool mStartupFinished = false;

  bool mCacheInitialized = false;
  bool mSaveComplete = false;
  bool mDataPrepared = false;
  // May only be changed on the main thread, while `mSaveMonitor` is held.
  bool mCacheInvalidated = false;

  // The list of scripts that we read from the initial startup cache file,
  // but have yet to initiate a decode task for.
  LinkedList<CachedStencil> mPendingScripts;

  // The lists of scripts and their sources that make up the chunk currently
  // being decoded in a background thread.
  JS::TranscodeSources mParsingSources;
  Vector<CachedStencil*> mParsingScripts;

  // The token for the completed off-thread decode task.
  Atomic<JS::OffThreadToken*, ReleaseAcquire> mToken{nullptr};

  // True if a runnable has been dispatched to the main thread to finish an
  // off-thread decode operation. Access only while 'mMonitor' is held.
  bool mFinishDecodeRunnablePending = false;

  // True is main-thread is blocked and we should notify with Monitor. Access
  // only while `mMonitor` is held.
  bool mWaitingForDecode = false;

  // The process type of the current process.
  static ProcessType sProcessType;

  // The process types for which remote processes have been initialized, and
  // are expected to send back script data.
  EnumSet<ProcessType> mInitializedProcesses{};

  RefPtr<ScriptPreloader> mChildCache;
  ScriptCacheChild* mChildActor = nullptr;

  nsString mBaseName;
  nsCString mContentStartupFinishedTopic;

  nsCOMPtr<nsIFile> mProfD;
  nsCOMPtr<nsIThread> mSaveThread;
  nsCOMPtr<nsITimer> mSaveTimer;

  // The mmapped cache data from this session's cache file.
  // The instance is held by either `gCacheData` or `gChildCacheData` static
  // fields, and its lifetime is guaranteed to be longer than ScriptPreloader
  // instance.
  AutoMemMap* mCacheData;

  Monitor mMonitor;
  Monitor mSaveMonitor;
};

}  // namespace mozilla

#endif  // ScriptPreloader_h
