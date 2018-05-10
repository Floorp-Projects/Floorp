/* -*-  Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScriptPreloader_h
#define ScriptPreloader_h

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
#include "nsIFile.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsIThread.h"

#include "jsapi.h"
#include "js/GCAnnotations.h"

#include <prio.h>

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
        Parent,
        Web,
        Extension,
    };

    template <typename T>
    struct Matcher
    {
        virtual bool Matches(T) = 0;
    };
}

using namespace mozilla::loader;

class ScriptPreloader : public nsIObserver
                      , public nsIMemoryReporter
                      , public nsIRunnable
{
    MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

    friend class mozilla::loader::ScriptCacheChild;

public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_NSIMEMORYREPORTER
    NS_DECL_NSIRUNNABLE

    static ScriptPreloader& GetSingleton();
    static ScriptPreloader& GetChildSingleton();

    static ProcessType GetChildProcessType(const nsAString& remoteType);

    // Retrieves the script with the given cache key from the script cache.
    // Returns null if the script is not cached.
    JSScript* GetCachedScript(JSContext* cx, const nsCString& name);

    // Notes the execution of a script with the given URL and cache key.
    // Depending on the stage of startup, the script may be serialized and
    // stored to the startup script cache.
    void NoteScript(const nsCString& url, const nsCString& cachePath, JS::HandleScript script);

    void NoteScript(const nsCString& url, const nsCString& cachePath,
                    ProcessType processType, nsTArray<uint8_t>&& xdrData,
                    TimeStamp loadTime);

    // Initializes the script cache from the startup script cache file.
    Result<Ok, nsresult> InitCache(const nsAString& = NS_LITERAL_STRING("scriptCache"));

    Result<Ok, nsresult> InitCache(const Maybe<ipc::FileDescriptor>& cacheFile, ScriptCacheChild* cacheChild);

    bool Active()
    {
      return mCacheInitialized && !mStartupFinished;
    }

private:
    Result<Ok, nsresult> InitCacheInternal(JS::HandleObject scope = nullptr);

public:
    void Trace(JSTracer* trc);

    static ProcessType CurrentProcessType()
    {
        return sProcessType;
    }

    static void InitContentChild(dom::ContentParent& parent);

protected:
    virtual ~ScriptPreloader() = default;

private:
    enum class ScriptStatus {
      Restored,
      Saved,
    };

    // Represents a cached JS script, either initially read from the script
    // cache file, to be added to the next session's script cache file, or
    // both.
    //
    // A script which was read from the cache file may be in any of the
    // following states:
    //
    //  - Read from the cache, and being compiled off thread. In this case,
    //    mReadyToExecute is false, and mToken is null.
    //  - Off-thread compilation has finished, but the script has not yet been
    //    executed. In this case, mReadyToExecute is true, and mToken has a non-null
    //    value.
    //  - Read from the cache, but too small or needed to immediately to be
    //    compiled off-thread. In this case, mReadyToExecute is true, and both mToken
    //    and mScript are null.
    //  - Fully decoded, and ready to be added to the next session's cache
    //    file. In this case, mReadyToExecute is true, and mScript is non-null.
    //
    // A script to be added to the next session's cache file always has a
    // non-null mScript value. If it was read from the last session's cache
    // file, it also has a non-empty mXDRRange range, which will be stored in
    // the next session's cache file. If it was compiled in this session, its
    // mXDRRange will initially be empty, and its mXDRData buffer will be
    // populated just before it is written to the cache file.
    class CachedScript : public LinkedListElement<CachedScript>
    {
    public:
        CachedScript(CachedScript&&) = delete;

        CachedScript(ScriptPreloader& cache, const nsCString& url, const nsCString& cachePath, JSScript* script)
            : mCache(cache)
            , mURL(url)
            , mCachePath(cachePath)
            , mScript(script)
            , mReadyToExecute(true)
        {}

        inline CachedScript(ScriptPreloader& cache, InputBuffer& buf);

        ~CachedScript() = default;

        ScriptStatus Status() const
        {
          return mProcessTypes.isEmpty() ? ScriptStatus::Restored : ScriptStatus::Saved;
        }

        // For use with nsTArray::Sort.
        //
        // Orders scripts by script load time, so that scripts which are needed
        // earlier are stored earlier, and scripts needed at approximately the
        // same time are stored approximately contiguously.
        struct Comparator
        {
            bool Equals(const CachedScript* a, const CachedScript* b) const
            {
              return a->mLoadTime == b->mLoadTime;
            }

            bool LessThan(const CachedScript* a, const CachedScript* b) const
            {
              return a->mLoadTime < b->mLoadTime;
            }
        };

        struct StatusMatcher final : public Matcher<CachedScript*>
        {
            explicit StatusMatcher(ScriptStatus status) : mStatus(status) {}

            virtual bool Matches(CachedScript* script) override
            {
                return script->Status() == mStatus;
            }

            const ScriptStatus mStatus;
        };

        void FreeData()
        {
            // If the script data isn't mmapped, we need to release both it
            // and the Range that points to it at the same time.
            if (!mXDRData.empty()) {
                mXDRRange.reset();
                mXDRData.destroy();
            }
        }

        void UpdateLoadTime(const TimeStamp& loadTime)
        {
          if (mLoadTime.IsNull() || loadTime < mLoadTime) {
            mLoadTime = loadTime;
          }
        }

        // Encodes this script into XDR data, and stores the result in mXDRData.
        // Returns true on success, false on failure.
        bool XDREncode(JSContext* cx);

        // Encodes or decodes this script, in the storage format required by the
        // script cache file.
        template<typename Buffer>
        void Code(Buffer& buffer)
        {
            buffer.codeString(mURL);
            buffer.codeString(mCachePath);
            buffer.codeUint32(mOffset);
            buffer.codeUint32(mSize);
            buffer.codeUint8(mProcessTypes);
        }

        // Returns the XDR data generated for this script during this session. See
        // mXDRData.
        JS::TranscodeBuffer& Buffer()
        {
            MOZ_ASSERT(HasBuffer());
            return mXDRData.ref<JS::TranscodeBuffer>();
        }

        bool HasBuffer() { return mXDRData.constructed<JS::TranscodeBuffer>(); }

        // Returns the read-only XDR data for this script. See mXDRRange.
        const JS::TranscodeRange& Range()
        {
            MOZ_ASSERT(HasRange());
            return mXDRRange.ref();
        }

        bool HasRange() { return mXDRRange.isSome(); }

        nsTArray<uint8_t>& Array()
        {
            MOZ_ASSERT(HasArray());
            return mXDRData.ref<nsTArray<uint8_t>>();
        }

        bool HasArray() { return mXDRData.constructed<nsTArray<uint8_t>>(); }


        JSScript* GetJSScript(JSContext* cx);

        size_t HeapSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf)
        {
            auto size = mallocSizeOf(this);

            if (HasArray()) {
                size += Array().ShallowSizeOfExcludingThis(mallocSizeOf);
            } else if (HasBuffer()) {
                size += Buffer().sizeOfExcludingThis(mallocSizeOf);
            } else {
                return size;
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

        JS::Heap<JSScript*> mScript;

        // True if this script is ready to be executed. This means that either the
        // off-thread portion of an off-thread decode has finished, or the script
        // is too small to be decoded off-thread, and may be immediately decoded
        // whenever it is first executed.
        bool mReadyToExecute = false;

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
        MaybeOneOf<JS::TranscodeBuffer, nsTArray<uint8_t>> mXDRData;
    } JS_HAZ_NON_GC_POINTER;

    template <ScriptStatus status>
    static Matcher<CachedScript*>* Match()
    {
        static CachedScript::StatusMatcher matcher{status};
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

    ScriptPreloader();

    void ForceWriteCacheFile();
    void Cleanup();

    void FinishPendingParses(MonitorAutoLock& aMal);
    void InvalidateCache();

    // Opens the cache file for reading.
    Result<Ok, nsresult> OpenCache();

    // Writes a new cache file to disk. Must not be called on the main thread.
    Result<Ok, nsresult> WriteCache();

    // Prepares scripts for writing to the cache, serializing new scripts to
    // XDR, and calculating their size-based offsets.
    void PrepareCacheWrite();

    void PrepareCacheWriteInternal();

    // Returns a file pointer for the cache file with the given name in the
    // current profile.
    Result<nsCOMPtr<nsIFile>, nsresult>
    GetCacheFile(const nsAString& suffix);

    // Waits for the given cached script to finish compiling off-thread, or
    // decodes it synchronously on the main thread, as appropriate.
    JSScript* WaitForCachedScript(JSContext* cx, CachedScript* script);

    void DecodeNextBatch(size_t chunkSize, JS::HandleObject scope = nullptr);

    static void OffThreadDecodeCallback(JS::OffThreadToken* token, void* context);
    void MaybeFinishOffThreadDecode();
    void DoFinishOffThreadDecode();

    // Returns the global scope object for off-thread compilation. When global
    // sharing is enabled in the component loader, this should be the shared
    // module global. Otherwise, it should be the XPConnect compilation scope.
    JSObject* CompilationScope(JSContext* cx);

    size_t ShallowHeapSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf)
    {
        return (mallocSizeOf(this) + mScripts.ShallowSizeOfExcludingThis(mallocSizeOf) +
                mallocSizeOf(mSaveThread.get()) + mallocSizeOf(mProfD.get()));
    }

    using ScriptHash = nsClassHashtable<nsCStringHashKey, CachedScript>;

    template<ScriptStatus status>
    static size_t SizeOfHashEntries(ScriptHash& scripts, mozilla::MallocSizeOf mallocSizeOf)
    {
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
    bool mCacheInvalidated = false;
    bool mBlockedOnSyncDispatch = false;

    // The list of scripts that we read from the initial startup cache file,
    // but have yet to initiate a decode task for.
    LinkedList<CachedScript> mPendingScripts;

    // The lists of scripts and their sources that make up the chunk currently
    // being decoded in a background thread.
    JS::TranscodeSources mParsingSources;
    Vector<CachedScript*> mParsingScripts;

    // The token for the completed off-thread decode task.
    JS::OffThreadToken* mToken = nullptr;

    // True if a runnable has been dispatched to the main thread to finish an
    // off-thread decode operation.
    bool mFinishDecodeRunnablePending = false;

    // The process type of the current process.
    static ProcessType sProcessType;

    // The process types for which remote processes have been initialized, and
    // are expected to send back script data.
    EnumSet<ProcessType> mInitializedProcesses{};

    RefPtr<ScriptPreloader> mChildCache;
    ScriptCacheChild* mChildActor = nullptr;

    nsString mBaseName;

    nsCOMPtr<nsIFile> mProfD;
    nsCOMPtr<nsIThread> mSaveThread;

    // The mmapped cache data from this session's cache file.
    AutoMemMap mCacheData;

    Monitor mMonitor;
    Monitor mSaveMonitor;
};

} // namespace mozilla

#endif // ScriptPreloader_h
