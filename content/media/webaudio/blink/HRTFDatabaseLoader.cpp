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

#include "HRTFDatabaseLoader.h"

#include "HRTFDatabase.h"

using namespace mozilla;

namespace WebCore {

// Singleton
nsTHashtable<HRTFDatabaseLoader::LoaderByRateEntry>*
    HRTFDatabaseLoader::s_loaderMap = nullptr;

TemporaryRef<HRTFDatabaseLoader> HRTFDatabaseLoader::createAndLoadAsynchronouslyIfNecessary(float sampleRate)
{
    MOZ_ASSERT(NS_IsMainThread());

    RefPtr<HRTFDatabaseLoader> loader;
    
    if (!s_loaderMap) {
        s_loaderMap = new nsTHashtable<LoaderByRateEntry>();
        s_loaderMap->Init();
    }

    LoaderByRateEntry* entry = s_loaderMap->PutEntry(sampleRate);
    loader = entry->mLoader;
    if (loader) { // existing entry
        MOZ_ASSERT(sampleRate == loader->databaseSampleRate());
        return loader;
    }

    loader = new HRTFDatabaseLoader(sampleRate);
    entry->mLoader = loader;

    loader->loadAsynchronously();

    return loader;
}

HRTFDatabaseLoader::HRTFDatabaseLoader(float sampleRate)
    : m_threadLock("HRTFDatabaseLoader")
    , m_databaseLoaderThread(nullptr)
    , m_databaseSampleRate(sampleRate)
{
    MOZ_ASSERT(NS_IsMainThread());
}

HRTFDatabaseLoader::~HRTFDatabaseLoader()
{
    MOZ_ASSERT(NS_IsMainThread());

    waitForLoaderThreadCompletion();
    m_hrtfDatabase.reset();

    // Remove ourself from the map.
    s_loaderMap->RemoveEntry(m_databaseSampleRate);
    if (s_loaderMap->Count() == 0) {
        delete s_loaderMap;
        s_loaderMap = nullptr;
    }
}

// Asynchronously load the database in this thread.
static void databaseLoaderEntry(void* threadData)
{
    PR_SetCurrentThreadName("HRTFDatabaseLdr");

    HRTFDatabaseLoader* loader = reinterpret_cast<HRTFDatabaseLoader*>(threadData);
    MOZ_ASSERT(loader);
    loader->load();
}

void HRTFDatabaseLoader::load()
{
    MOZ_ASSERT(!NS_IsMainThread());
    if (!m_hrtfDatabase.get()) {
        // Load the default HRTF database.
        m_hrtfDatabase = HRTFDatabase::create(m_databaseSampleRate);
    }
}

void HRTFDatabaseLoader::loadAsynchronously()
{
    MOZ_ASSERT(NS_IsMainThread());

    MutexAutoLock locker(m_threadLock);
    
    if (!m_hrtfDatabase.get() && !m_databaseLoaderThread) {
        // Start the asynchronous database loading process.
        m_databaseLoaderThread =
            PR_CreateThread(PR_USER_THREAD, databaseLoaderEntry, this,
                            PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                            PR_JOINABLE_THREAD, 0);
    }
}

bool HRTFDatabaseLoader::isLoaded() const
{
    return m_hrtfDatabase.get();
}

void HRTFDatabaseLoader::waitForLoaderThreadCompletion()
{
    MutexAutoLock locker(m_threadLock);
    
    // waitForThreadCompletion() should not be called twice for the same thread.
    if (m_databaseLoaderThread) {
        DebugOnly<PRStatus> status = PR_JoinThread(m_databaseLoaderThread);
        MOZ_ASSERT(status == PR_SUCCESS, "PR_JoinThread failed");
    }
    m_databaseLoaderThread = nullptr;
}

} // namespace WebCore
