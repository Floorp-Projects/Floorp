/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ScriptPreloader.h"
#include "ScriptPreloader-inl.h"
#include "mozilla/loader/ScriptCacheActors.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"

#include "MainThreadUtils.h"
#include "nsDebug.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsJSUtils.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "xpcpublic.h"

#define DELAYED_STARTUP_TOPIC "browser-delayed-startup-finished"
#define DOC_ELEM_INSERTED_TOPIC "document-element-inserted"
#define CLEANUP_TOPIC "xpcom-shutdown"
#define SHUTDOWN_TOPIC "quit-application-granted"
#define CACHE_FLUSH_TOPIC "startupcache-invalidate"

namespace mozilla {
namespace {
static LazyLogModule gLog("ScriptPreloader");

#define LOG(level, ...) MOZ_LOG(gLog, LogLevel::level, (__VA_ARGS__))
}

using mozilla::dom::AutoJSAPI;
using mozilla::dom::ContentChild;
using mozilla::dom::ContentParent;
using namespace mozilla::loader;

ProcessType ScriptPreloader::sProcessType;


nsresult
ScriptPreloader::CollectReports(nsIHandleReportCallback* aHandleReport,
                                nsISupports* aData, bool aAnonymize)
{
    MOZ_COLLECT_REPORT(
        "explicit/script-preloader/heap/saved-scripts", KIND_HEAP, UNITS_BYTES,
        SizeOfLinkedList(mSavedScripts, MallocSizeOf),
        "Memory used to hold the scripts which have been executed in this "
        "session, and will be written to the startup script cache file.");

    MOZ_COLLECT_REPORT(
        "explicit/script-preloader/heap/restored-scripts", KIND_HEAP, UNITS_BYTES,
        SizeOfLinkedList(mRestoredScripts, MallocSizeOf),
        "Memory used to hold the scripts which have been restored from the "
        "startup script cache file, but have not been executed in this session.");

    MOZ_COLLECT_REPORT(
        "explicit/script-preloader/heap/other", KIND_HEAP, UNITS_BYTES,
        ShallowHeapSizeOfIncludingThis(MallocSizeOf),
        "Memory used by the script cache service itself.");

    MOZ_COLLECT_REPORT(
        "explicit/script-preloader/non-heap/memmapped-cache", KIND_NONHEAP, UNITS_BYTES,
        mCacheData.nonHeapSizeOfExcludingThis(),
        "The memory-mapped startup script cache file.");

    return NS_OK;
}


ScriptPreloader&
ScriptPreloader::GetSingleton()
{
    static RefPtr<ScriptPreloader> singleton;

    if (!singleton) {
        if (XRE_IsParentProcess()) {
            singleton = new ScriptPreloader();
            singleton->mChildCache = &GetChildSingleton();
            Unused << singleton->InitCache();
        } else {
            singleton = &GetChildSingleton();
        }

        ClearOnShutdown(&singleton);
    }

    return *singleton;
}

// The child singleton is available in all processes, including the parent, and
// is used for scripts which are expected to be loaded into child processes
// (such as process and frame scripts), or scripts that have already been loaded
// into a child. The child caches are managed as follows:
//
// - Every startup, we open the cache file from the last session, move it to a
//  new location, and begin pre-loading the scripts that are stored in it. There
//  is a separate cache file for parent and content processes, but the parent
//  process opens both the parent and content cache files.
//
// - Once startup is complete, we write a new cache file for the next session,
//   containing only the scripts that were used during early startup, so we don't
//   waste pre-loading scripts that may not be needed.
//
// - For content processes, opening and writing the cache file is handled in the
//  parent process. The first content process of each type sends back the data
//  for scripts that were loaded in early startup, and the parent merges them and
//  writes them to a cache file.
//
// - Currently, content processes only benefit from the cache data written
//  during the *previous* session. Ideally, new content processes should probably
//  use the cache data written during this session if there was no previous cache
//  file, but I'd rather do that as a follow-up.
ScriptPreloader&
ScriptPreloader::GetChildSingleton()
{
    static RefPtr<ScriptPreloader> singleton;

    if (!singleton) {
        singleton = new ScriptPreloader();
        if (XRE_IsParentProcess()) {
            Unused << singleton->InitCache(NS_LITERAL_STRING("scriptCache-child"));
        }
        ClearOnShutdown(&singleton);
    }

    return *singleton;
}

void
ScriptPreloader::InitContentChild(ContentParent& parent)
{
    auto& cache = GetChildSingleton();

    // We want startup script data from the first process of a given type.
    // That process sends back its script data before it executes any
    // untrusted code, and then we never accept further script data for that
    // type of process for the rest of the session.
    //
    // The script data from each process type is merged with the data from the
    // parent process's frame and process scripts, and shared between all
    // content process types in the next session.
    //
    // Note that if the first process of a given type crashes or shuts down
    // before sending us its script data, we silently ignore it, and data for
    // that process type is not included in the next session's cache. This
    // should be a sufficiently rare occurrence that it's not worth trying to
    // handle specially.
    auto processType = GetChildProcessType(parent.GetRemoteType());
    bool wantScriptData = !cache.mInitializedProcesses.contains(processType);
    cache.mInitializedProcesses += processType;

    auto fd = cache.mCacheData.cloneFileDescriptor();
    if (fd.IsValid()) {
        Unused << parent.SendPScriptCacheConstructor(fd, wantScriptData);
    } else {
        Unused << parent.SendPScriptCacheConstructor(NS_ERROR_FILE_NOT_FOUND, wantScriptData);
    }
}

ProcessType
ScriptPreloader::GetChildProcessType(const nsAString& remoteType)
{
    if (remoteType.EqualsLiteral(EXTENSION_REMOTE_TYPE)) {
        return ProcessType::Extension;
    }
    return ProcessType::Web;
}


namespace {

static void
TraceOp(JSTracer* trc, void* data)
{
    auto preloader = static_cast<ScriptPreloader*>(data);

    preloader->Trace(trc);
}

} // anonymous namespace

void
ScriptPreloader::Trace(JSTracer* trc)
{
    for (auto script : mSavedScripts) {
        JS::TraceEdge(trc, &script->mScript, "ScriptPreloader::CachedScript.mScript");
    }

    for (auto script : mRestoredScripts) {
        JS::TraceEdge(trc, &script->mScript, "ScriptPreloader::CachedScript.mScript");
    }
}


ScriptPreloader::ScriptPreloader()
  : mMonitor("[ScriptPreloader.mMonitor]")
  , mSaveMonitor("[ScriptPreloader.mSaveMonitor]")
{
    if (XRE_IsParentProcess()) {
        sProcessType = ProcessType::Parent;
    } else {
        sProcessType = GetChildProcessType(dom::ContentChild::GetSingleton()->GetRemoteType());
    }

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    MOZ_RELEASE_ASSERT(obs);

    if (XRE_IsParentProcess()) {
        // In the parent process, we want to freeze the script cache as soon
        // as delayed startup for the first browser window has completed.
        obs->AddObserver(this, DELAYED_STARTUP_TOPIC, false);
    } else {
        // In the child process, we need to freeze the script cache before any
        // untrusted code has been executed. The insertion of the first DOM
        // document element may sometimes be earlier than is ideal, but at
        // least it should always be safe.
        obs->AddObserver(this, DOC_ELEM_INSERTED_TOPIC, false);
    }
    obs->AddObserver(this, SHUTDOWN_TOPIC, false);
    obs->AddObserver(this, CLEANUP_TOPIC, false);
    obs->AddObserver(this, CACHE_FLUSH_TOPIC, false);

    AutoSafeJSAPI jsapi;
    JS_AddExtraGCRootsTracer(jsapi.cx(), TraceOp, this);
}

void
ScriptPreloader::ForceWriteCacheFile()
{
    if (mSaveThread) {
        MonitorAutoLock mal(mSaveMonitor);

        // Unblock the save thread, so it can start saving before we get to
        // XPCOM shutdown.
        mal.Notify();
    }
}

void
ScriptPreloader::Cleanup()
{
    if (mSaveThread) {
        MonitorAutoLock mal(mSaveMonitor);

        while (!mSaveComplete && mSaveThread) {
            mal.Wait();
        }
    }

    mSavedScripts.clear();
    mRestoredScripts.clear();

    AutoSafeJSAPI jsapi;
    JS_RemoveExtraGCRootsTracer(jsapi.cx(), TraceOp, this);

    UnregisterWeakMemoryReporter(this);
}

void
ScriptPreloader::FlushScripts(LinkedList<CachedScript>& scripts)
{
    for (auto next = scripts.getFirst(); next; ) {
        auto script = next;
        next = script->getNext();

        // We can only purge finished scripts here. Async scripts that are
        // still being parsed off-thread have a non-refcounted reference to
        // this script, which needs to stay alive until they finish parsing.
        if (script->mReadyToExecute) {
            script->Cancel();
            script->remove();
            delete script;
        }
    }
}

void
ScriptPreloader::FlushCache()
{
    MonitorAutoLock mal(mMonitor);

    FlushScripts(mSavedScripts);
    FlushScripts(mRestoredScripts);

    // If we've already finished saving the cache at this point, start a new
    // delayed save operation. This will write out an empty cache file in place
    // of any cache file we've already written out this session, which will
    // prevent us from falling back to the current session's cache file on the
    // next startup.
    if (mSaveComplete && mChildCache) {
        mSaveComplete = false;

        Unused << NS_NewNamedThread("SaveScripts",
                                    getter_AddRefs(mSaveThread), this);
    }
}

nsresult
ScriptPreloader::Observe(nsISupports* subject, const char* topic, const char16_t* data)
{
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (!strcmp(topic, DELAYED_STARTUP_TOPIC)) {
        obs->RemoveObserver(this, DELAYED_STARTUP_TOPIC);

        MOZ_ASSERT(XRE_IsParentProcess());

        mStartupFinished = true;

        if (mChildCache) {
            Unused << NS_NewNamedThread("SaveScripts",
                                        getter_AddRefs(mSaveThread), this);
        }
    } else if (!strcmp(topic, DOC_ELEM_INSERTED_TOPIC)) {
        obs->RemoveObserver(this, DOC_ELEM_INSERTED_TOPIC);

        MOZ_ASSERT(XRE_IsContentProcess());

        mStartupFinished = true;

        if (mChildActor) {
            mChildActor->Finalize(mSavedScripts);
        }
    } else if (!strcmp(topic, SHUTDOWN_TOPIC)) {
        ForceWriteCacheFile();
    } else if (!strcmp(topic, CLEANUP_TOPIC)) {
        Cleanup();
    } else if (!strcmp(topic, CACHE_FLUSH_TOPIC)) {
        FlushCache();
    }

    return NS_OK;
}


Result<nsCOMPtr<nsIFile>, nsresult>
ScriptPreloader::GetCacheFile(const nsAString& suffix)
{
    nsCOMPtr<nsIFile> cacheFile;
    NS_TRY(mProfD->Clone(getter_AddRefs(cacheFile)));

    NS_TRY(cacheFile->AppendNative(NS_LITERAL_CSTRING("startupCache")));
    Unused << cacheFile->Create(nsIFile::DIRECTORY_TYPE, 0777);

    NS_TRY(cacheFile->Append(mBaseName + suffix));

    return Move(cacheFile);
}

static const uint8_t MAGIC[] = "mozXDRcachev001";

Result<Ok, nsresult>
ScriptPreloader::OpenCache()
{
    NS_TRY(NS_GetSpecialDirectory("ProfLDS", getter_AddRefs(mProfD)));

    nsCOMPtr<nsIFile> cacheFile;
    MOZ_TRY_VAR(cacheFile, GetCacheFile(NS_LITERAL_STRING(".bin")));

    bool exists;
    NS_TRY(cacheFile->Exists(&exists));
    if (exists) {
        NS_TRY(cacheFile->MoveTo(nullptr, mBaseName + NS_LITERAL_STRING("-current.bin")));
    } else {
        NS_TRY(cacheFile->SetLeafName(mBaseName + NS_LITERAL_STRING("-current.bin")));
        NS_TRY(cacheFile->Exists(&exists));
        if (!exists) {
            return Err(NS_ERROR_FILE_NOT_FOUND);
        }
    }

    MOZ_TRY(mCacheData.init(cacheFile));

    return Ok();
}

// Opens the script cache file for this session, and initializes the script
// cache based on its contents. See WriteCache for details of the cache file.
Result<Ok, nsresult>
ScriptPreloader::InitCache(const nsAString& basePath)
{
    mCacheInitialized = true;
    mBaseName = basePath;

    RegisterWeakMemoryReporter(this);

    if (!XRE_IsParentProcess()) {
        return Ok();
    }

    MOZ_TRY(OpenCache());

    return InitCacheInternal();
}

Result<Ok, nsresult>
ScriptPreloader::InitCache(const Maybe<ipc::FileDescriptor>& cacheFile, ScriptCacheChild* cacheChild)
{
    MOZ_ASSERT(XRE_IsContentProcess());

    mCacheInitialized = true;
    mChildActor = cacheChild;

    RegisterWeakMemoryReporter(this);

    if (cacheFile.isNothing()){
        return Ok();
    }

    MOZ_TRY(mCacheData.init(cacheFile.ref()));

    return InitCacheInternal();
}

Result<Ok, nsresult>
ScriptPreloader::InitCacheInternal()
{
    auto size = mCacheData.size();

    uint32_t headerSize;
    if (size < sizeof(MAGIC) + sizeof(headerSize)) {
        return Err(NS_ERROR_UNEXPECTED);
    }

    auto data = mCacheData.get<uint8_t>();
    auto end = data + size;

    if (memcmp(MAGIC, data.get(), sizeof(MAGIC))) {
        return Err(NS_ERROR_UNEXPECTED);
    }
    data += sizeof(MAGIC);

    headerSize = LittleEndian::readUint32(data.get());
    data += sizeof(headerSize);

    if (data + headerSize > end) {
        return Err(NS_ERROR_UNEXPECTED);
    }

    {
        AutoCleanLinkedList<CachedScript> scripts;

        Range<uint8_t> header(data, data + headerSize);
        data += headerSize;

        InputBuffer buf(header);

        size_t offset = 0;
        while (!buf.finished()) {
            auto script = MakeUnique<CachedScript>(*this, buf);

            auto scriptData = data + script->mOffset;
            if (scriptData + script->mSize > end) {
                return Err(NS_ERROR_UNEXPECTED);
            }

            // Make sure offsets match what we'd expect based on script ordering and
            // size, as a basic sanity check.
            if (script->mOffset != offset) {
                return Err(NS_ERROR_UNEXPECTED);
            }
            offset += script->mSize;

            script->mXDRRange.emplace(scriptData, scriptData + script->mSize);

            scripts.insertBack(script.release());
        }

        if (buf.error()) {
            return Err(NS_ERROR_UNEXPECTED);
        }

        for (auto script : scripts) {
            mScripts.Put(script->mCachePath, script);
        }
        mRestoredScripts = Move(scripts);
    }

    AutoJSAPI jsapi;
    MOZ_RELEASE_ASSERT(jsapi.Init(xpc::CompilationScope()));
    JSContext* cx = jsapi.cx();

    auto start = TimeStamp::Now();
    LOG(Info, "Off-thread decoding scripts...\n");

    JS::CompileOptions options(cx, JSVERSION_LATEST);
    for (auto script : mRestoredScripts) {
        // Only async decode scripts which have been used in this process type.
        if (script->mProcessTypes.contains(CurrentProcessType()) &&
            script->AsyncDecodable() &&
            JS::CanCompileOffThread(cx, options, script->mSize)) {
            DecodeScriptOffThread(cx, script);
        } else {
            script->mReadyToExecute = true;
        }
    }

    LOG(Info, "Initialized decoding in %fms\n",
        (TimeStamp::Now() - start).ToMilliseconds());

    return Ok();
}

static inline Result<Ok, nsresult>
Write(PRFileDesc* fd, const void* data, int32_t len)
{
    if (PR_Write(fd, data, len) != len) {
        return Err(NS_ERROR_FAILURE);
    }
    return Ok();
}

void
ScriptPreloader::PrepareCacheWrite()
{
    MOZ_ASSERT(NS_IsMainThread());

    auto cleanup = MakeScopeExit([&] () {
        if (mChildCache) {
            mChildCache->PrepareCacheWrite();
        }
    });

    if (mDataPrepared) {
        return;
    }

    if (mRestoredScripts.isEmpty()) {
        // Check for any new scripts that we need to save. If there aren't
        // any, and there aren't any saved scripts that we need to remove,
        // don't bother writing out a new cache file.
        bool found = false;
        for (auto script : mSavedScripts) {
            if (!script->HasRange() || script->HasArray()) {
                found = true;
                break;
            }
        }
        if (!found) {
            mSaveComplete = true;
            return;
        }
    }

    AutoSafeJSAPI jsapi;

    for (CachedScript* next = mSavedScripts.getFirst(); next; ) {
        CachedScript* script = next;
        next = script->getNext();

        // Don't write any scripts that are also in the child cache. They'll be
        // loaded from the child cache in that case, so there's no need to write
        // them twice.
        CachedScript* childScript = mChildCache ? mChildCache->mScripts.Get(script->mCachePath) : nullptr;
        if (childScript) {
            if (FindScript(mChildCache->mSavedScripts, script->mCachePath)) {
                childScript->UpdateLoadTime(script->mLoadTime);
                childScript->mProcessTypes += script->mProcessTypes;
            } else {
                childScript = nullptr;
            }
        }

        if (childScript || (!script->mSize && !script->XDREncode(jsapi.cx()))) {
            script->remove();
            delete script;
        } else {
            script->mSize = script->Range().length();
        }
    }

    mDataPrepared = true;
}

// Writes out a script cache file for the scripts accessed during early
// startup in this session. The cache file is a little-endian binary file with
// the following format:
//
// - A uint32 containing the size of the header block.
//
// - A header entry for each file stored in the cache containing:
//   - The URL that the script was originally read from.
//   - Its cache key.
//   - The offset of its XDR data within the XDR data block.
//   - The size of its XDR data in the XDR data block.
//   - A bit field describing which process types the script is used in.
//
// - A block of XDR data for the encoded scripts, with each script's data at
//   an offset from the start of the block, as specified above.
Result<Ok, nsresult>
ScriptPreloader::WriteCache()
{
    MOZ_ASSERT(!NS_IsMainThread());

    if (!mDataPrepared && !mSaveComplete) {
        MonitorAutoUnlock mau(mSaveMonitor);

        NS_DispatchToMainThread(
            NewRunnableMethod(this, &ScriptPreloader::PrepareCacheWrite),
            NS_DISPATCH_SYNC);
    }

    if (mSaveComplete) {
        // If we don't have anything we need to save, we're done.
        return Ok();
    }

    nsCOMPtr<nsIFile> cacheFile;
    MOZ_TRY_VAR(cacheFile, GetCacheFile(NS_LITERAL_STRING("-new.bin")));

    bool exists;
    NS_TRY(cacheFile->Exists(&exists));
    if (exists) {
        NS_TRY(cacheFile->Remove(false));
    }

    AutoFDClose fd;
    NS_TRY(cacheFile->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE, 0644, &fd.rwget()));

    nsTArray<CachedScript*> scripts;
    for (auto script : mSavedScripts) {
        scripts.AppendElement(script);
    }

    // Sort scripts by load time, with async loaded scripts before sync scripts.
    // Since async scripts are always loaded immediately at startup, it helps to
    // have them stored contiguously.
    scripts.Sort(CachedScript::Comparator());

    OutputBuffer buf;
    size_t offset = 0;
    for (auto script : scripts) {
        script->mOffset = offset;
        script->Code(buf);

        offset += script->mSize;
    }

    uint8_t headerSize[4];
    LittleEndian::writeUint32(headerSize, buf.cursor());

    MOZ_TRY(Write(fd, MAGIC, sizeof(MAGIC)));
    MOZ_TRY(Write(fd, headerSize, sizeof(headerSize)));
    MOZ_TRY(Write(fd, buf.Get(), buf.cursor()));
    for (auto script : scripts) {
        MOZ_TRY(Write(fd, script->Range().begin().get(), script->mSize));

        if (script->mScript) {
            script->FreeData();
        }
    }

    NS_TRY(cacheFile->MoveTo(nullptr, mBaseName + NS_LITERAL_STRING(".bin")));

    return Ok();
}

// Runs in the mSaveThread thread, and writes out the cache file for the next
// session after a reasonable delay.
nsresult
ScriptPreloader::Run()
{
    MonitorAutoLock mal(mSaveMonitor);

    // Ideally wait about 10 seconds before saving, to avoid unnecessary IO
    // during early startup.
    mal.Wait(10000);

    auto result = WriteCache();
    Unused << NS_WARN_IF(result.isErr());

    result = mChildCache->WriteCache();
    Unused << NS_WARN_IF(result.isErr());

    mSaveComplete = true;
    NS_ReleaseOnMainThread(mSaveThread.forget());

    mal.NotifyAll();
    return NS_OK;
}

/* static */ ScriptPreloader::CachedScript*
ScriptPreloader::FindScript(LinkedList<CachedScript>& scripts, const nsCString& cachePath)
{
    for (auto script : scripts) {
        if (script->mCachePath == cachePath) {
            return script;
        }
    }
    return nullptr;
}

void
ScriptPreloader::NoteScript(const nsCString& url, const nsCString& cachePath,
                            JS::HandleScript jsscript)
{
    // Don't bother trying to cache any URLs with cache-busting query
    // parameters.
    if (mStartupFinished || !mCacheInitialized || cachePath.FindChar('?') >= 0) {
        return;
    }

    // Don't bother caching files that belong to the mochitest harness.
    NS_NAMED_LITERAL_CSTRING(mochikitPrefix, "chrome://mochikit/");
    if (StringHead(url, mochikitPrefix.Length()) == mochikitPrefix) {
        return;
    }

    CachedScript* script = mScripts.Get(cachePath);
    bool restored = script && FindScript(mRestoredScripts, cachePath);

    if (restored) {
        script->remove();
        mSavedScripts.insertBack(script);

        MOZ_ASSERT(jsscript);
        script->mScript = jsscript;
        script->mReadyToExecute = true;
    } else if (!script) {
        script = new CachedScript(*this, url, cachePath, jsscript);
        mSavedScripts.insertBack(script);
        mScripts.Put(cachePath, script);
    } else {
        return;
    }

    script->UpdateLoadTime(TimeStamp::Now());
    script->mProcessTypes += CurrentProcessType();
}

void
ScriptPreloader::NoteScript(const nsCString& url, const nsCString& cachePath,
                            ProcessType processType, nsTArray<uint8_t>&& xdrData,
                            TimeStamp loadTime)
{
    CachedScript* script = mScripts.Get(cachePath);
    bool restored = script && FindScript(mRestoredScripts, cachePath);

    if (restored) {
        script->remove();
        mSavedScripts.insertBack(script);

        script->mReadyToExecute = true;
    } else {
        if (!script) {
            script = new CachedScript(this, url, cachePath, nullptr);
            mSavedScripts.insertBack(script);
            mScripts.Put(cachePath, script);
        }

        if (!script->HasRange()) {
            MOZ_ASSERT(!script->HasArray());

            script->mSize = xdrData.Length();
            script->mXDRData.construct<nsTArray<uint8_t>>(Forward<nsTArray<uint8_t>>(xdrData));

            auto& data = script->Array();
            script->mXDRRange.emplace(data.Elements(), data.Length());
        }
    }

    script->UpdateLoadTime(loadTime);
    script->mProcessTypes += processType;
}

JSScript*
ScriptPreloader::GetCachedScript(JSContext* cx, const nsCString& path)
{
    // If a script is used by both the parent and the child, it's stored only
    // in the child cache.
    if (mChildCache) {
        auto script = mChildCache->GetCachedScript(cx, path);
        if (script) {
            return script;
        }
    }

    auto script = mScripts.Get(path);
    if (script) {
        return WaitForCachedScript(cx, script);
    }

    return nullptr;
}

JSScript*
ScriptPreloader::WaitForCachedScript(JSContext* cx, CachedScript* script)
{
    if (!script->mReadyToExecute) {
        LOG(Info, "Must wait for async script load: %s\n", script->mURL.get());
        auto start = TimeStamp::Now();

        MonitorAutoLock mal(mMonitor);

        if (!script->mReadyToExecute && script->mSize < MAX_MAINTHREAD_DECODE_SIZE) {
            LOG(Info, "Script is small enough to recompile on main thread\n");

            script->mReadyToExecute = true;
        } else {
            while (!script->mReadyToExecute) {
                mal.Wait();
            }
        }

        LOG(Info, "Waited %fms\n", (TimeStamp::Now() - start).ToMilliseconds());
    }

    return script->GetJSScript(cx);
}


void
ScriptPreloader::DecodeScriptOffThread(JSContext* cx, CachedScript* script)
{
    JS::CompileOptions options(cx, JSVERSION_LATEST);

    options.setNoScriptRval(true)
           .setFileAndLine(script->mURL.get(), 1);

    if (!JS::DecodeOffThreadScript(cx, options, script->Range(),
                                   OffThreadDecodeCallback,
                                   static_cast<void*>(script))) {
        script->mReadyToExecute = true;
    }
}

void
ScriptPreloader::CancelOffThreadParse(void* token)
{
    AutoSafeJSAPI jsapi;
    JS::CancelOffThreadScriptDecoder(jsapi.cx(), token);
}

/* static */ void
ScriptPreloader::OffThreadDecodeCallback(void* token, void* context)
{
    auto script = static_cast<CachedScript*>(context);

    MonitorAutoLock mal(script->mCache.mMonitor);

    if (script->mReadyToExecute) {
        // We've already executed this script on the main thread, and opted to
        // main thread decode it rather waiting for off-thread decoding to
        // finish. So just cancel the off-thread parse rather than completing
        // it.
        NS_DispatchToMainThread(
            NewRunnableMethod<void*>(&script->mCache,
                                     &ScriptPreloader::CancelOffThreadParse,
                                     token));
        return;
    }

    script->mToken = token;
    script->mReadyToExecute = true;

    mal.NotifyAll();
}

ScriptPreloader::CachedScript::CachedScript(ScriptPreloader& cache, InputBuffer& buf)
    : mCache(cache)
{
    Code(buf);
}

bool
ScriptPreloader::CachedScript::XDREncode(JSContext* cx)
{
    JSAutoCompartment ac(cx, mScript);
    JS::RootedScript jsscript(cx, mScript);

    mXDRData.construct<JS::TranscodeBuffer>();

    JS::TranscodeResult code = JS::EncodeScript(cx, Buffer(), jsscript);
    if (code == JS::TranscodeResult_Ok) {
        mXDRRange.emplace(Buffer().begin(), Buffer().length());
        return true;
    }
    JS_ClearPendingException(cx);
    return false;
}

void
ScriptPreloader::CachedScript::Cancel()
{
    if (mToken) {
        mCache.mMonitor.AssertCurrentThreadOwns();

        AutoSafeJSAPI jsapi;
        JS::CancelOffThreadScriptDecoder(jsapi.cx(), mToken);

        mReadyToExecute = true;
        mToken = nullptr;
    }
}

JSScript*
ScriptPreloader::CachedScript::GetJSScript(JSContext* cx)
{
    MOZ_ASSERT(mReadyToExecute);
    if (mScript) {
        return mScript;
    }

    // If we have no token at this point, the script was too small to decode
    // off-thread, or it was needed before the off-thread compilation was
    // finished, and is small enough to decode on the main thread rather than
    // wait for the off-thread decoding to finish. In either case, we decode
    // it synchronously the first time it's needed.
    if (!mToken) {
        MOZ_ASSERT(HasRange());

        JS::RootedScript script(cx);
        if (JS::DecodeScript(cx, Range(), &script)) {
            mScript = script;

            if (mCache.mSaveComplete) {
                FreeData();
            }
        }

        return mScript;
    }

    Maybe<JSAutoCompartment> ac;
    if (JS::CompartmentCreationOptionsRef(cx).addonIdOrNull()) {
        // Make sure we never try to finish the parse in a compartment with an
        // add-on ID, it wasn't started in one.
        ac.emplace(cx, xpc::CompilationScope());
    }

    mScript = JS::FinishOffThreadScriptDecoder(cx, mToken);
    mToken = nullptr;
    return mScript;
}

NS_IMPL_ISUPPORTS(ScriptPreloader, nsIObserver, nsIRunnable, nsIMemoryReporter)

#undef LOG

} // namespace mozilla
