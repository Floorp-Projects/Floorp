/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HRTFDatabaseLoader_h
#define HRTFDatabaseLoader_h

#include "nsHashKeys.h"
#include "mozilla/RefPtr.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "HRTFDatabase.h"

template <class EntryType> class nsTHashtable;
template <class T> class nsAutoRef;

namespace WebCore {

// HRTFDatabaseLoader will asynchronously load the default HRTFDatabase in a new thread.

class HRTFDatabaseLoader {
public:
    // Lazily creates a HRTFDatabaseLoader (if not already created) for the given sample-rate
    // and starts loading asynchronously (when created the first time).
    // Returns the HRTFDatabaseLoader.
    // Must be called from the main thread.
    static already_AddRefed<HRTFDatabaseLoader> createAndLoadAsynchronouslyIfNecessary(float sampleRate);

    // AddRef and Release may be called from any thread.
    void AddRef()
    {
#if defined(DEBUG) || defined(NS_BUILD_REFCNT_LOGGING)
        int count =
#endif
          ++m_refCnt;
        MOZ_ASSERT(count > 0, "invalid ref count");
        NS_LOG_ADDREF(this, count, "HRTFDatabaseLoader", sizeof(*this));
    }

    void Release()
    {
        // The last reference can't be removed on a non-main thread because
        // the object can be accessed on the main thread from the hash
        // table via createAndLoadAsynchronouslyIfNecessary().
        int count = m_refCnt;
        MOZ_ASSERT(count > 0, "extra release");
        // Optimization attempt to possibly skip proxying the release to the
        // main thread.
        if (count != 1 && m_refCnt.compareExchange(count, count - 1)) {
            NS_LOG_RELEASE(this, count - 1, "HRTFDatabaseLoader");
            return;
        }

        ProxyRelease();
    }

    // Returns true once the default database has been completely loaded.
    bool isLoaded() const;

    // waitForLoaderThreadCompletion() may be called more than once,
    // on any thread except m_databaseLoaderThread.
    void waitForLoaderThreadCompletion();

    HRTFDatabase* database() { return m_hrtfDatabase.get(); }

    float databaseSampleRate() const { return m_databaseSampleRate; }

    static void shutdown();

    // Called in asynchronous loading thread.
    void load();

    // Sums the size of all cached database loaders.
    static size_t sizeOfLoaders(mozilla::MallocSizeOf aMallocSizeOf);

private:
    // Both constructor and destructor must be called from the main thread.
    explicit HRTFDatabaseLoader(float sampleRate);
    ~HRTFDatabaseLoader();

    size_t sizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

    void ProxyRelease(); // any thread
    void MainThreadRelease(); // main thread only
    class ProxyReleaseEvent;

    // If it hasn't already been loaded, creates a new thread and initiates asynchronous loading of the default database.
    // This must be called from the main thread.
    void loadAsynchronously();

    // Map from sample-rate to loader.
    class LoaderByRateEntry : public nsFloatHashKey {
    public:
        explicit LoaderByRateEntry(KeyTypePointer aKey)
            : nsFloatHashKey(aKey)
            , mLoader() // so PutEntry() will zero-initialize
        {
        }

        size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
        {
            return mLoader ? mLoader->sizeOfIncludingThis(aMallocSizeOf) : 0;
        }

        HRTFDatabaseLoader* mLoader;
    };

    // Keeps track of loaders on a per-sample-rate basis.
    static nsTHashtable<LoaderByRateEntry> *s_loaderMap; // singleton

    mozilla::Atomic<int> m_refCnt;

    nsAutoRef<HRTFDatabase> m_hrtfDatabase;

    // Holding a m_threadLock is required when accessing m_databaseLoaderThread.
    mozilla::Mutex m_threadLock;
    PRThread* m_databaseLoaderThread;

    float m_databaseSampleRate;
};

} // namespace WebCore

#endif // HRTFDatabaseLoader_h
