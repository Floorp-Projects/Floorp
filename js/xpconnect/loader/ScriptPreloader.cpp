/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptPreloader-inl.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Monitor.h"

#include "mozilla/ScriptPreloader.h"
#include "mozilla/loader/ScriptCacheActors.h"

#include "mozilla/URLPreloader.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Components.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/FileUtils.h"
#include "mozilla/IOBuffers.h"
#include "mozilla/Logging.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_javascript.h"
#include "mozilla/TaskController.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Try.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Document.h"
#include "mozilla/scache/StartupCache.h"

#include "crc32c.h"
#include "js/CompileOptions.h"              // JS::ReadOnlyCompileOptions
#include "js/experimental/JSStencil.h"      // JS::Stencil, JS::DecodeStencil
#include "js/experimental/CompileScript.h"  // JS::NewFrontendContext, JS::DestroyFrontendContext, JS::SetNativeStackQuota, JS::ThreadStackQuotaForSize
#include "js/Transcoding.h"
#include "MainThreadUtils.h"
#include "nsDebug.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsJSUtils.h"
#include "nsMemoryReporterManager.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "xpcpublic.h"

#define STARTUP_COMPLETE_TOPIC "browser-delayed-startup-finished"
#define DOC_ELEM_INSERTED_TOPIC "document-element-inserted"
#define CONTENT_DOCUMENT_LOADED_TOPIC "content-document-loaded"
#define CACHE_WRITE_TOPIC "browser-idle-startup-tasks-finished"
#define XPCOM_SHUTDOWN_TOPIC "xpcom-shutdown"
#define CACHE_INVALIDATE_TOPIC "startupcache-invalidate"

// The maximum time we'll wait for a child process to finish starting up before
// we send its script data back to the parent.
constexpr uint32_t CHILD_STARTUP_TIMEOUT_MS = 8000;

namespace mozilla {
namespace {
static LazyLogModule gLog("ScriptPreloader");

#define LOG(level, ...) MOZ_LOG(gLog, LogLevel::level, (__VA_ARGS__))
}  // namespace

using mozilla::dom::AutoJSAPI;
using mozilla::dom::ContentChild;
using mozilla::dom::ContentParent;
using namespace mozilla::loader;
using mozilla::scache::StartupCache;

using namespace JS;

ProcessType ScriptPreloader::sProcessType;

nsresult ScriptPreloader::CollectReports(nsIHandleReportCallback* aHandleReport,
                                         nsISupports* aData, bool aAnonymize) {
  MOZ_COLLECT_REPORT(
      "explicit/script-preloader/heap/saved-scripts", KIND_HEAP, UNITS_BYTES,
      SizeOfHashEntries<ScriptStatus::Saved>(mScripts, MallocSizeOf),
      "Memory used to hold the scripts which have been executed in this "
      "session, and will be written to the startup script cache file.");

  MOZ_COLLECT_REPORT(
      "explicit/script-preloader/heap/restored-scripts", KIND_HEAP, UNITS_BYTES,
      SizeOfHashEntries<ScriptStatus::Restored>(mScripts, MallocSizeOf),
      "Memory used to hold the scripts which have been restored from the "
      "startup script cache file, but have not been executed in this session.");

  MOZ_COLLECT_REPORT("explicit/script-preloader/heap/other", KIND_HEAP,
                     UNITS_BYTES, ShallowHeapSizeOfIncludingThis(MallocSizeOf),
                     "Memory used by the script cache service itself.");

  // Since the mem-mapped cache file is mapped into memory, we want to report
  // it as explicit memory somewhere. But since the child cache is shared
  // between all processes, we don't want to report it as explicit memory for
  // all of them. So we report it as explicit only in the parent process, and
  // non-explicit everywhere else.
  if (XRE_IsParentProcess()) {
    MOZ_COLLECT_REPORT("explicit/script-preloader/non-heap/memmapped-cache",
                       KIND_NONHEAP, UNITS_BYTES,
                       mCacheData->nonHeapSizeOfExcludingThis(),
                       "The memory-mapped startup script cache file.");
  } else {
    MOZ_COLLECT_REPORT("script-preloader-memmapped-cache", KIND_NONHEAP,
                       UNITS_BYTES, mCacheData->nonHeapSizeOfExcludingThis(),
                       "The memory-mapped startup script cache file.");
  }

  return NS_OK;
}

StaticRefPtr<ScriptPreloader> ScriptPreloader::gScriptPreloader;
StaticRefPtr<ScriptPreloader> ScriptPreloader::gChildScriptPreloader;
StaticAutoPtr<AutoMemMap> ScriptPreloader::gCacheData;
StaticAutoPtr<AutoMemMap> ScriptPreloader::gChildCacheData;

ScriptPreloader& ScriptPreloader::GetSingleton() {
  if (!gScriptPreloader) {
    if (XRE_IsParentProcess()) {
      gCacheData = new AutoMemMap();
      gScriptPreloader = new ScriptPreloader(gCacheData.get());
      gScriptPreloader->mChildCache = &GetChildSingleton();
      Unused << gScriptPreloader->InitCache();
    } else {
      gScriptPreloader = &GetChildSingleton();
    }
  }

  return *gScriptPreloader;
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
//   containing only the scripts that were used during early startup, so we
//   don't waste pre-loading scripts that may not be needed.
//
// - For content processes, opening and writing the cache file is handled in the
//  parent process. The first content process of each type sends back the data
//  for scripts that were loaded in early startup, and the parent merges them
//  and writes them to a cache file.
//
// - Currently, content processes only benefit from the cache data written
//  during the *previous* session. Ideally, new content processes should
//  probably use the cache data written during this session if there was no
//  previous cache file, but I'd rather do that as a follow-up.
ScriptPreloader& ScriptPreloader::GetChildSingleton() {
  if (!gChildScriptPreloader) {
    gChildCacheData = new AutoMemMap();
    gChildScriptPreloader = new ScriptPreloader(gChildCacheData.get());
    if (XRE_IsParentProcess()) {
      Unused << gChildScriptPreloader->InitCache(u"scriptCache-child"_ns);
    }
  }

  return *gChildScriptPreloader;
}

/* static */
void ScriptPreloader::DeleteSingleton() {
  gScriptPreloader = nullptr;
  gChildScriptPreloader = nullptr;
}

/* static */
void ScriptPreloader::DeleteCacheDataSingleton() {
  MOZ_ASSERT(!gScriptPreloader);
  MOZ_ASSERT(!gChildScriptPreloader);

  gCacheData = nullptr;
  gChildCacheData = nullptr;
}

void ScriptPreloader::InitContentChild(ContentParent& parent) {
  auto& cache = GetChildSingleton();
  cache.mSaveMonitor.AssertOnWritingThread();

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

  auto fd = cache.mCacheData->cloneFileDescriptor();
  // Don't send original cache data to new processes if the cache has been
  // invalidated.
  if (fd.IsValid() && !cache.mCacheInvalidated) {
    Unused << parent.SendPScriptCacheConstructor(fd, wantScriptData);
  } else {
    Unused << parent.SendPScriptCacheConstructor(NS_ERROR_FILE_NOT_FOUND,
                                                 wantScriptData);
  }
}

ProcessType ScriptPreloader::GetChildProcessType(const nsACString& remoteType) {
  if (remoteType == EXTENSION_REMOTE_TYPE) {
    return ProcessType::Extension;
  }
  if (remoteType == PRIVILEGEDABOUT_REMOTE_TYPE) {
    return ProcessType::PrivilegedAbout;
  }
  return ProcessType::Web;
}

ScriptPreloader::ScriptPreloader(AutoMemMap* cacheData)
    : mCacheData(cacheData),
      mMonitor("[ScriptPreloader.mMonitor]"),
      mSaveMonitor("[ScriptPreloader.mSaveMonitor]", this) {
  // We do not set the process type for child processes here because the
  // remoteType in ContentChild is not ready yet.
  if (XRE_IsParentProcess()) {
    sProcessType = ProcessType::Parent;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  MOZ_RELEASE_ASSERT(obs);

  if (XRE_IsParentProcess()) {
    // In the parent process, we want to freeze the script cache as soon
    // as idle tasks for the first browser window have completed.
    obs->AddObserver(this, STARTUP_COMPLETE_TOPIC, false);
    obs->AddObserver(this, CACHE_WRITE_TOPIC, false);
  }

  obs->AddObserver(this, XPCOM_SHUTDOWN_TOPIC, false);
  obs->AddObserver(this, CACHE_INVALIDATE_TOPIC, false);
}

ScriptPreloader::~ScriptPreloader() { Cleanup(); }

void ScriptPreloader::Cleanup() {
  mScripts.Clear();
  UnregisterWeakMemoryReporter(this);
}

void ScriptPreloader::StartCacheWrite() {
  MOZ_DIAGNOSTIC_ASSERT(!mSaveThread);

  Unused << NS_NewNamedThread("SaveScripts", getter_AddRefs(mSaveThread), this);

  nsCOMPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();
  barrier->AddBlocker(this, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__,
                      u""_ns);
}

void ScriptPreloader::InvalidateCache() {
  {
    mMonitor.AssertNotCurrentThreadOwns();
    MonitorAutoLock mal(mMonitor);

    // Wait for pending off-thread parses to finish, since they depend on the
    // memory allocated by our CachedStencil, and can't be canceled
    // asynchronously.
    FinishPendingParses(mal);

    // Pending scripts should have been cleared by the above, and the queue
    // should have been reset.
    MOZ_ASSERT(mDecodingScripts.isEmpty());
    MOZ_ASSERT(!mDecodedStencils);

    mScripts.Clear();

    // If we've already finished saving the cache at this point, start a new
    // delayed save operation. This will write out an empty cache file in place
    // of any cache file we've already written out this session, which will
    // prevent us from falling back to the current session's cache file on the
    // next startup.
    if (mSaveComplete && !mSaveThread && mChildCache) {
      mSaveComplete = false;

      StartCacheWrite();
    }
  }

  {
    MonitorSingleWriterAutoLock saveMonitorAutoLock(mSaveMonitor);

    mCacheInvalidated = true;
  }

  // If we're waiting on a timeout to finish saving, interrupt it and just save
  // immediately.
  mSaveMonitor.NotifyAll();
}

nsresult ScriptPreloader::Observe(nsISupports* subject, const char* topic,
                                  const char16_t* data) {
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!strcmp(topic, STARTUP_COMPLETE_TOPIC)) {
    obs->RemoveObserver(this, STARTUP_COMPLETE_TOPIC);

    MOZ_ASSERT(XRE_IsParentProcess());

    mStartupFinished = true;
    URLPreloader::GetSingleton().SetStartupFinished();
  } else if (!strcmp(topic, CACHE_WRITE_TOPIC)) {
    obs->RemoveObserver(this, CACHE_WRITE_TOPIC);

    MOZ_ASSERT(mStartupFinished);
    MOZ_ASSERT(XRE_IsParentProcess());

    if (mChildCache && !mSaveComplete && !mSaveThread) {
      StartCacheWrite();
    }
  } else if (mContentStartupFinishedTopic.Equals(topic)) {
    // If this is an uninitialized about:blank viewer or a chrome: document
    // (which should always be an XBL binding document), ignore it. We don't
    // have to worry about it loading malicious content.
    if (nsCOMPtr<dom::Document> doc = do_QueryInterface(subject)) {
      nsCOMPtr<nsIURI> uri = doc->GetDocumentURI();

      if ((NS_IsAboutBlank(uri) &&
           doc->GetReadyStateEnum() == doc->READYSTATE_UNINITIALIZED) ||
          uri->SchemeIs("chrome")) {
        return NS_OK;
      }
    }
    FinishContentStartup();
  } else if (!strcmp(topic, "timer-callback")) {
    FinishContentStartup();
  } else if (!strcmp(topic, XPCOM_SHUTDOWN_TOPIC)) {
    // Wait for any pending parses to finish at this point, to avoid creating
    // new stencils during destroying the JS runtime.
    MonitorAutoLock mal(mMonitor);
    FinishPendingParses(mal);
  } else if (!strcmp(topic, CACHE_INVALIDATE_TOPIC)) {
    InvalidateCache();
  }

  return NS_OK;
}

void ScriptPreloader::FinishContentStartup() {
  MOZ_ASSERT(XRE_IsContentProcess());

#ifdef DEBUG
  if (mContentStartupFinishedTopic.Equals(CONTENT_DOCUMENT_LOADED_TOPIC)) {
    MOZ_ASSERT(sProcessType == ProcessType::PrivilegedAbout);
  } else {
    MOZ_ASSERT(sProcessType != ProcessType::PrivilegedAbout);
  }
#endif /* DEBUG */

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  obs->RemoveObserver(this, mContentStartupFinishedTopic.get());

  mSaveTimer = nullptr;

  mStartupFinished = true;

  if (mChildActor) {
    mChildActor->SendScriptsAndFinalize(mScripts);
  }

#ifdef XP_WIN
  // Record the amount of USS at startup. This is Windows-only for now,
  // we could turn it on for Linux relatively cheaply. On macOS it can have
  // a perf impact. Only record this for non-privileged processes because
  // privileged processes record this value at a different time, leading to
  // a higher value which skews the telemetry.
  if (sProcessType != ProcessType::PrivilegedAbout) {
    mozilla::Telemetry::Accumulate(
        mozilla::Telemetry::MEMORY_UNIQUE_CONTENT_STARTUP,
        nsMemoryReporterManager::ResidentUnique() / 1024);
  }
#endif
}

bool ScriptPreloader::WillWriteScripts() {
  return !mDataPrepared && (XRE_IsParentProcess() || mChildActor);
}

Result<nsCOMPtr<nsIFile>, nsresult> ScriptPreloader::GetCacheFile(
    const nsAString& suffix) {
  NS_ENSURE_TRUE(mProfD, Err(NS_ERROR_NOT_INITIALIZED));

  nsCOMPtr<nsIFile> cacheFile;
  MOZ_TRY(mProfD->Clone(getter_AddRefs(cacheFile)));

  MOZ_TRY(cacheFile->AppendNative("startupCache"_ns));
  Unused << cacheFile->Create(nsIFile::DIRECTORY_TYPE, 0777);

  MOZ_TRY(cacheFile->Append(mBaseName + suffix));

  return std::move(cacheFile);
}

static const uint8_t MAGIC[] = "mozXDRcachev003";

Result<Ok, nsresult> ScriptPreloader::OpenCache() {
  if (StartupCache::GetIgnoreDiskCache()) {
    return Err(NS_ERROR_ABORT);
  }

  MOZ_TRY(NS_GetSpecialDirectory("ProfLDS", getter_AddRefs(mProfD)));

  nsCOMPtr<nsIFile> cacheFile;
  MOZ_TRY_VAR(cacheFile, GetCacheFile(u".bin"_ns));

  bool exists;
  MOZ_TRY(cacheFile->Exists(&exists));
  if (exists) {
    MOZ_TRY(cacheFile->MoveTo(nullptr, mBaseName + u"-current.bin"_ns));
  } else {
    MOZ_TRY(cacheFile->SetLeafName(mBaseName + u"-current.bin"_ns));
    MOZ_TRY(cacheFile->Exists(&exists));
    if (!exists) {
      return Err(NS_ERROR_FILE_NOT_FOUND);
    }
  }

  MOZ_TRY(mCacheData->init(cacheFile));

  return Ok();
}

// Opens the script cache file for this session, and initializes the script
// cache based on its contents. See WriteCache for details of the cache file.
Result<Ok, nsresult> ScriptPreloader::InitCache(const nsAString& basePath) {
  mSaveMonitor.AssertOnWritingThread();
  mCacheInitialized = true;
  mBaseName = basePath;

  RegisterWeakMemoryReporter(this);

  if (!XRE_IsParentProcess()) {
    return Ok();
  }

  // Grab the compilation scope before initializing the URLPreloader, since
  // it's not safe to run component loader code during its critical section.
  AutoSafeJSAPI jsapi;
  JS::RootedObject scope(jsapi.cx(), xpc::CompilationScope());

  // Note: Code on the main thread *must not access Omnijar in any way* until
  // this AutoBeginReading guard is destroyed.
  URLPreloader::AutoBeginReading abr;

  MOZ_TRY(OpenCache());

  return InitCacheInternal(scope);
}

Result<Ok, nsresult> ScriptPreloader::InitCache(
    const Maybe<ipc::FileDescriptor>& cacheFile, ScriptCacheChild* cacheChild) {
  mSaveMonitor.AssertOnWritingThread();
  MOZ_ASSERT(XRE_IsContentProcess());

  mCacheInitialized = true;
  mChildActor = cacheChild;
  sProcessType =
      GetChildProcessType(dom::ContentChild::GetSingleton()->GetRemoteType());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  MOZ_RELEASE_ASSERT(obs);

  if (sProcessType == ProcessType::PrivilegedAbout) {
    // Since we control all of the documents loaded in the privileged
    // content process, we can increase the window of active time for the
    // ScriptPreloader to include the scripts that are loaded until the
    // first document finishes loading.
    mContentStartupFinishedTopic.AssignLiteral(CONTENT_DOCUMENT_LOADED_TOPIC);
  } else {
    // In the child process, we need to freeze the script cache before any
    // untrusted code has been executed. The insertion of the first DOM
    // document element may sometimes be earlier than is ideal, but at
    // least it should always be safe.
    mContentStartupFinishedTopic.AssignLiteral(DOC_ELEM_INSERTED_TOPIC);
  }
  obs->AddObserver(this, mContentStartupFinishedTopic.get(), false);

  RegisterWeakMemoryReporter(this);

  auto cleanup = MakeScopeExit([&] {
    // If the parent is expecting cache data from us, make sure we send it
    // before it writes out its cache file. For normal proceses, this isn't
    // a concern, since they begin loading documents quite early. For the
    // preloaded process, we may end up waiting a long time (or, indeed,
    // never loading a document), so we need an additional timeout.
    if (cacheChild) {
      NS_NewTimerWithObserver(getter_AddRefs(mSaveTimer), this,
                              CHILD_STARTUP_TIMEOUT_MS,
                              nsITimer::TYPE_ONE_SHOT);
    }
  });

  if (cacheFile.isNothing()) {
    return Ok();
  }

  MOZ_TRY(mCacheData->init(cacheFile.ref()));

  return InitCacheInternal();
}

Result<Ok, nsresult> ScriptPreloader::InitCacheInternal(
    JS::HandleObject scope) {
  auto size = mCacheData->size();

  uint32_t headerSize;
  uint32_t crc;
  if (size < sizeof(MAGIC) + sizeof(headerSize) + sizeof(crc)) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  auto data = mCacheData->get<uint8_t>();
  MOZ_RELEASE_ASSERT(JS::IsTranscodingBytecodeAligned(data.get()));

  auto end = data + size;

  if (memcmp(MAGIC, data.get(), sizeof(MAGIC))) {
    return Err(NS_ERROR_UNEXPECTED);
  }
  data += sizeof(MAGIC);

  headerSize = LittleEndian::readUint32(data.get());
  data += sizeof(headerSize);

  crc = LittleEndian::readUint32(data.get());
  data += sizeof(crc);

  if (data + headerSize > end) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  if (crc != ComputeCrc32c(~0, data.get(), headerSize)) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  {
    auto cleanup = MakeScopeExit([&]() { mScripts.Clear(); });

    LinkedList<CachedStencil> scripts;

    Range<uint8_t> header(data, data + headerSize);
    data += headerSize;

    // Reconstruct alignment padding if required.
    size_t currentOffset = data - mCacheData->get<uint8_t>();
    data += JS::AlignTranscodingBytecodeOffset(currentOffset) - currentOffset;

    InputBuffer buf(header);

    size_t offset = 0;
    while (!buf.finished()) {
      auto script = MakeUnique<CachedStencil>(*this, buf);
      MOZ_RELEASE_ASSERT(script);

      auto scriptData = data + script->mOffset;
      if (!JS::IsTranscodingBytecodeAligned(scriptData.get())) {
        return Err(NS_ERROR_UNEXPECTED);
      }

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

      // Don't pre-decode the script unless it was used in this process type
      // during the previous session.
      if (script->mOriginalProcessTypes.contains(CurrentProcessType())) {
        scripts.insertBack(script.get());
      } else {
        script->mReadyToExecute = true;
      }

      const auto& cachePath = script->mCachePath;
      mScripts.InsertOrUpdate(cachePath, std::move(script));
    }

    if (buf.error()) {
      return Err(NS_ERROR_UNEXPECTED);
    }

    mDecodingScripts = std::move(scripts);
    cleanup.release();
  }

  StartDecodeTask(scope);
  return Ok();
}

void ScriptPreloader::PrepareCacheWriteInternal() {
  MOZ_ASSERT(NS_IsMainThread());

  mMonitor.AssertCurrentThreadOwns();

  auto cleanup = MakeScopeExit([&]() {
    if (mChildCache) {
      mChildCache->PrepareCacheWrite();
    }
  });

  if (mDataPrepared) {
    return;
  }

  AutoSafeJSAPI jsapi;
  JSAutoRealm ar(jsapi.cx(), xpc::PrivilegedJunkScope());
  bool found = false;
  for (auto& script : IterHash(mScripts, Match<ScriptStatus::Saved>())) {
    // Don't write any scripts that are also in the child cache. They'll be
    // loaded from the child cache in that case, so there's no need to write
    // them twice.
    CachedStencil* childScript =
        mChildCache ? mChildCache->mScripts.Get(script->mCachePath) : nullptr;
    if (childScript && !childScript->mProcessTypes.isEmpty()) {
      childScript->UpdateLoadTime(script->mLoadTime);
      childScript->mProcessTypes += script->mProcessTypes;
      script.Remove();
      continue;
    }

    if (!(script->mProcessTypes == script->mOriginalProcessTypes)) {
      // Note: EnumSet doesn't support operator!=, hence the weird form above.
      found = true;
    }

    if (!script->mSize && !script->XDREncode(jsapi.cx())) {
      script.Remove();
    }
  }

  if (!found) {
    mSaveComplete = true;
    return;
  }

  mDataPrepared = true;
}

void ScriptPreloader::PrepareCacheWrite() {
  MonitorAutoLock mal(mMonitor);

  PrepareCacheWriteInternal();
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
Result<Ok, nsresult> ScriptPreloader::WriteCache() {
  MOZ_ASSERT(!NS_IsMainThread());
  mSaveMonitor.AssertCurrentThreadOwns();

  if (!mDataPrepared && !mSaveComplete) {
    MonitorSingleWriterAutoUnlock mau(mSaveMonitor);

    NS_DispatchAndSpinEventLoopUntilComplete(
        "ScriptPreloader::PrepareCacheWrite"_ns,
        GetMainThreadSerialEventTarget(),
        NewRunnableMethod("ScriptPreloader::PrepareCacheWrite", this,
                          &ScriptPreloader::PrepareCacheWrite));
  }

  if (mSaveComplete) {
    // If we don't have anything we need to save, we're done.
    return Ok();
  }

  nsCOMPtr<nsIFile> cacheFile;
  MOZ_TRY_VAR(cacheFile, GetCacheFile(u"-new.bin"_ns));

  bool exists;
  MOZ_TRY(cacheFile->Exists(&exists));
  if (exists) {
    MOZ_TRY(cacheFile->Remove(false));
  }

  {
    AutoFDClose fd;
    MOZ_TRY(cacheFile->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE, 0644,
                                        &fd.rwget()));

    // We also need to hold mMonitor while we're touching scripts in
    // mScripts, or they may be freed before we're done with them.
    mMonitor.AssertNotCurrentThreadOwns();
    MonitorAutoLock mal(mMonitor);

    nsTArray<CachedStencil*> scripts;
    for (auto& script : IterHash(mScripts, Match<ScriptStatus::Saved>())) {
      scripts.AppendElement(script);
    }

    // Sort scripts by load time, with async loaded scripts before sync scripts.
    // Since async scripts are always loaded immediately at startup, it helps to
    // have them stored contiguously.
    scripts.Sort(CachedStencil::Comparator());

    OutputBuffer buf;
    size_t offset = 0;
    for (auto script : scripts) {
      script->mOffset = offset;
      MOZ_DIAGNOSTIC_ASSERT(
          JS::IsTranscodingBytecodeOffsetAligned(script->mOffset));
      script->Code(buf);

      offset += script->mSize;
      MOZ_DIAGNOSTIC_ASSERT(
          JS::IsTranscodingBytecodeOffsetAligned(script->mSize));
    }

    uint8_t headerSize[4];
    LittleEndian::writeUint32(headerSize, buf.cursor());

    uint8_t crc[4];
    LittleEndian::writeUint32(crc, ComputeCrc32c(~0, buf.Get(), buf.cursor()));

    MOZ_TRY(Write(fd, MAGIC, sizeof(MAGIC)));
    MOZ_TRY(Write(fd, headerSize, sizeof(headerSize)));
    MOZ_TRY(Write(fd, crc, sizeof(crc)));
    MOZ_TRY(Write(fd, buf.Get(), buf.cursor()));

    // Align the start of the scripts section to the transcode alignment.
    size_t written = sizeof(MAGIC) + sizeof(headerSize) + buf.cursor();
    size_t padding = JS::AlignTranscodingBytecodeOffset(written) - written;
    if (padding) {
      MOZ_TRY(WritePadding(fd, padding));
      written += padding;
    }

    for (auto script : scripts) {
      MOZ_DIAGNOSTIC_ASSERT(JS::IsTranscodingBytecodeOffsetAligned(written));
      MOZ_TRY(Write(fd, script->Range().begin().get(), script->mSize));

      written += script->mSize;
      // We can only free the XDR data if the stencil isn't borrowing data from
      // it.
      if (script->mStencil && !JS::StencilIsBorrowed(script->mStencil)) {
        script->FreeData();
      }
    }
  }

  MOZ_TRY(cacheFile->MoveTo(nullptr, mBaseName + u".bin"_ns));

  return Ok();
}

nsresult ScriptPreloader::GetName(nsACString& aName) {
  aName.AssignLiteral("ScriptPreloader");
  return NS_OK;
}

// Runs in the mSaveThread thread, and writes out the cache file for the next
// session after a reasonable delay.
nsresult ScriptPreloader::Run() {
  MonitorSingleWriterAutoLock mal(mSaveMonitor);

  // Ideally wait about 10 seconds before saving, to avoid unnecessary IO
  // during early startup. But only if the cache hasn't been invalidated,
  // since that can trigger a new write during shutdown, and we don't want to
  // cause shutdown hangs.
  if (!mCacheInvalidated) {
    mal.Wait(TimeDuration::FromSeconds(10));
  }

  auto result = URLPreloader::GetSingleton().WriteCache();
  Unused << NS_WARN_IF(result.isErr());

  result = WriteCache();
  Unused << NS_WARN_IF(result.isErr());

  {
    MonitorSingleWriterAutoLock lock(mChildCache->mSaveMonitor);
    result = mChildCache->WriteCache();
  }
  Unused << NS_WARN_IF(result.isErr());

  NS_DispatchToMainThread(
      NewRunnableMethod("ScriptPreloader::CacheWriteComplete", this,
                        &ScriptPreloader::CacheWriteComplete),
      NS_DISPATCH_NORMAL);
  return NS_OK;
}

void ScriptPreloader::CacheWriteComplete() {
  mSaveThread->AsyncShutdown();
  mSaveThread = nullptr;
  mSaveComplete = true;

  nsCOMPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();
  barrier->RemoveBlocker(this);
}

void ScriptPreloader::NoteStencil(const nsCString& url,
                                  const nsCString& cachePath,
                                  JS::Stencil* stencil, bool isRunOnce) {
  if (!Active()) {
    if (isRunOnce) {
      if (auto script = mScripts.Get(cachePath)) {
        script->mIsRunOnce = true;
        script->MaybeDropStencil();
      }
    }
    return;
  }

  // Don't bother trying to cache any URLs with cache-busting query
  // parameters.
  if (cachePath.FindChar('?') >= 0) {
    return;
  }

  // Don't bother caching files that belong to the mochitest harness.
  constexpr auto mochikitPrefix = "chrome://mochikit/"_ns;
  if (StringHead(url, mochikitPrefix.Length()) == mochikitPrefix) {
    return;
  }

  auto* script =
      mScripts.GetOrInsertNew(cachePath, *this, url, cachePath, stencil);
  if (isRunOnce) {
    script->mIsRunOnce = true;
  }

  if (!script->MaybeDropStencil() && !script->mStencil) {
    MOZ_ASSERT(stencil);
    script->mStencil = stencil;
    script->mReadyToExecute = true;
  }

  script->UpdateLoadTime(TimeStamp::Now());
  script->mProcessTypes += CurrentProcessType();
}

void ScriptPreloader::NoteStencil(const nsCString& url,
                                  const nsCString& cachePath,
                                  ProcessType processType,
                                  nsTArray<uint8_t>&& xdrData,
                                  TimeStamp loadTime) {
  // After data has been prepared, there's no point in noting further scripts,
  // since the cache either has already been written, or is about to be
  // written. Any time prior to the data being prepared, we can safely mutate
  // mScripts without locking. After that point, the save thread is free to
  // access it, and we can't alter it without locking.
  if (mDataPrepared) {
    return;
  }

  auto* script =
      mScripts.GetOrInsertNew(cachePath, *this, url, cachePath, nullptr);

  if (!script->HasRange()) {
    MOZ_ASSERT(!script->HasArray());

    script->mSize = xdrData.Length();
    script->mXDRData.construct<nsTArray<uint8_t>>(
        std::forward<nsTArray<uint8_t>>(xdrData));

    auto& data = script->Array();
    script->mXDRRange.emplace(data.Elements(), data.Length());
  }

  if (!script->mSize && !script->mStencil) {
    // If the content process is sending us an entry for a stencil
    // which was in the cache at startup, it expects us to already have this
    // script data, so it doesn't send it.
    //
    // However, the cache may have been invalidated at this point (usually
    // due to the add-on manager installing or uninstalling a legacy
    // extension during very early startup), which means we may no longer
    // have an entry for this script. Since that means we have no data to
    // write to the new cache, and no JSScript to generate it from, we need
    // to discard this entry.
    mScripts.Remove(cachePath);
    return;
  }

  script->UpdateLoadTime(loadTime);
  script->mProcessTypes += processType;
}

/* static */
void ScriptPreloader::FillCompileOptionsForCachedStencil(
    JS::CompileOptions& options) {
  // Users of the cache do not require return values, so inform the JS parser in
  // order for it to generate simpler bytecode.
  options.setNoScriptRval(true);

  // The ScriptPreloader trades off having bytecode available but not source
  // text. This means the JS syntax-only parser is not used. If `toString` is
  // called on functions in these scripts, the source-hook will fetch it over,
  // so using `toString` of functions should be avoided in chrome js.
  options.setSourceIsLazy(true);
}

/* static */
void ScriptPreloader::FillDecodeOptionsForCachedStencil(
    JS::DecodeOptions& options) {
  // ScriptPreloader's XDR buffer is alive during the Stencil is alive.
  // The decoded stencil can borrow from it.
  //
  // NOTE: The XDR buffer is alive during the entire browser lifetime only
  //       when it's mmapped.
  options.borrowBuffer = true;
}

already_AddRefed<JS::Stencil> ScriptPreloader::GetCachedStencil(
    JSContext* cx, const JS::ReadOnlyDecodeOptions& options,
    const nsCString& path) {
  MOZ_RELEASE_ASSERT(
      !(XRE_IsContentProcess() && !mCacheInitialized),
      "ScriptPreloader must be initialized before getting cached "
      "scripts in the content process.");

  // If a script is used by both the parent and the child, it's stored only
  // in the child cache.
  if (mChildCache) {
    RefPtr<JS::Stencil> stencil =
        mChildCache->GetCachedStencilInternal(cx, options, path);
    if (stencil) {
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_SCRIPT_PRELOADER_REQUESTS::HitChild);
      return stencil.forget();
    }
  }

  RefPtr<JS::Stencil> stencil = GetCachedStencilInternal(cx, options, path);
  Telemetry::AccumulateCategorical(
      stencil ? Telemetry::LABELS_SCRIPT_PRELOADER_REQUESTS::Hit
              : Telemetry::LABELS_SCRIPT_PRELOADER_REQUESTS::Miss);
  return stencil.forget();
}

already_AddRefed<JS::Stencil> ScriptPreloader::GetCachedStencilInternal(
    JSContext* cx, const JS::ReadOnlyDecodeOptions& options,
    const nsCString& path) {
  auto* cachedScript = mScripts.Get(path);
  if (cachedScript) {
    return WaitForCachedStencil(cx, options, cachedScript);
  }
  return nullptr;
}

already_AddRefed<JS::Stencil> ScriptPreloader::WaitForCachedStencil(
    JSContext* cx, const JS::ReadOnlyDecodeOptions& options,
    CachedStencil* script) {
  if (!script->mReadyToExecute) {
    MOZ_ASSERT(mDecodedStencils);

    // Check for the finished operations that can contain our target.
    if (mDecodedStencils->AvailableRead() > 0) {
      FinishOffThreadDecode();
    }

    if (!script->mReadyToExecute) {
      // Our target is not yet decoded.

      // If script is small enough, we'd rather decode on main-thread than wait
      // for a decode task to complete.
      if (script->mSize < MAX_MAINTHREAD_DECODE_SIZE) {
        LOG(Info, "Script is small enough to recompile on main thread\n");

        script->mReadyToExecute = true;
        Telemetry::ScalarAdd(
            Telemetry::ScalarID::SCRIPT_PRELOADER_MAINTHREAD_RECOMPILE, 1);
      } else {
        LOG(Info, "Must wait for async script load: %s\n", script->mURL.get());
        auto start = TimeStamp::Now();

        MonitorAutoLock mal(mMonitor);

        // Process finished tasks until our target is found.
        while (!script->mReadyToExecute) {
          if (mDecodedStencils->AvailableRead() > 0) {
            FinishOffThreadDecode();
          } else {
            MOZ_ASSERT(!mDecodingScripts.isEmpty());
            mWaitingForDecode = true;
            mal.Wait();
            mWaitingForDecode = false;
          }
        }

        double waitedMS = (TimeStamp::Now() - start).ToMilliseconds();
        Telemetry::Accumulate(Telemetry::SCRIPT_PRELOADER_WAIT_TIME,
                              int(waitedMS));
        LOG(Debug, "Waited %fms\n", waitedMS);
      }
    }
  }

  return script->GetStencil(cx, options);
}

void ScriptPreloader::onDecodedStencilQueued() {
  mMonitor.AssertNotCurrentThreadOwns();
  MonitorAutoLock mal(mMonitor);

  if (mWaitingForDecode) {
    // Wake up the blocked main thread.
    mal.Notify();
  }

  // NOTE: Do not perform DoFinishOffThreadDecode for partial data.
}

void ScriptPreloader::OnDecodeTaskFinished() {
  mMonitor.AssertNotCurrentThreadOwns();
  MonitorAutoLock mal(mMonitor);

  if (mWaitingForDecode) {
    // Wake up the blocked main thread.
    mal.Notify();
  } else {
    // Issue a Runnable to handle all decoded stencils, even if the next
    // WaitForCachedStencil call has not happened yet.
    NS_DispatchToMainThread(
        NewRunnableMethod("ScriptPreloader::DoFinishOffThreadDecode", this,
                          &ScriptPreloader::DoFinishOffThreadDecode));
  }
}

void ScriptPreloader::OnDecodeTaskFailed() {
  // NOTE: nullptr is enqueued to mDecodedStencils, and FinishOffThreadDecode
  //       handles it as failure.
  OnDecodeTaskFinished();
}

void ScriptPreloader::FinishPendingParses(MonitorAutoLock& aMal) {
  mMonitor.AssertCurrentThreadOwns();

  MOZ_ASSERT_IF(!mDecodingScripts.isEmpty(), mDecodedStencils);

  // Process any pending decodes that are in flight.
  while (!mDecodingScripts.isEmpty()) {
    if (mDecodedStencils->AvailableRead() > 0) {
      FinishOffThreadDecode();
    } else {
      mWaitingForDecode = true;
      aMal.Wait();
      mWaitingForDecode = false;
    }
  }
}

void ScriptPreloader::DoFinishOffThreadDecode() {
  // NOTE: mDecodedStencils could already be reset.
  if (mDecodedStencils && mDecodedStencils->AvailableRead() > 0) {
    FinishOffThreadDecode();
  }
}

void ScriptPreloader::FinishOffThreadDecode() {
  MOZ_ASSERT(mDecodedStencils);

  while (mDecodedStencils->AvailableRead() > 0) {
    RefPtr<JS::Stencil> stencil;
    DebugOnly<int> reads = mDecodedStencils->Dequeue(&stencil, 1);
    MOZ_ASSERT(reads == 1);

    if (!stencil) {
      // DecodeTask failed.
      // Mark all remaining scripts to be decoded on the main thread.
      for (CachedStencil* next = mDecodingScripts.getFirst(); next;) {
        auto* script = next;
        next = script->getNext();

        script->mReadyToExecute = true;
        script->remove();
      }

      break;
    }

    CachedStencil* script = mDecodingScripts.getFirst();
    MOZ_ASSERT(script);

    LOG(Debug, "Finished off-thread decode of %s\n", script->mURL.get());
    script->mStencil = stencil.forget();
    script->mReadyToExecute = true;
    script->remove();
  }

  if (mDecodingScripts.isEmpty()) {
    mDecodedStencils.reset();
  }
}

void ScriptPreloader::StartDecodeTask(JS::HandleObject scope) {
  auto start = TimeStamp::Now();
  LOG(Debug, "Off-thread decoding scripts...\n");

  Vector<JS::TranscodeSource> decodingSources;

  size_t size = 0;
  for (CachedStencil* next = mDecodingScripts.getFirst(); next;) {
    auto* script = next;
    next = script->getNext();

    MOZ_ASSERT(script->IsMemMapped());

    // Skip any scripts that we decoded on the main thread rather than
    // waiting for an off-thread operation to complete.
    if (script->mReadyToExecute) {
      script->remove();
      continue;
    }
    if (!decodingSources.emplaceBack(script->Range(), script->mURL.get(), 0)) {
      break;
    }

    LOG(Debug, "Beginning off-thread decode of script %s (%u bytes)\n",
        script->mURL.get(), script->mSize);

    size += script->mSize;
  }

  MOZ_ASSERT(decodingSources.length() == mDecodingScripts.length());

  if (size == 0 && mDecodingScripts.isEmpty()) {
    return;
  }

  AutoSafeJSAPI jsapi;
  JSContext* cx = jsapi.cx();
  JSAutoRealm ar(cx, scope ? scope : xpc::CompilationScope());

  JS::CompileOptions options(cx);
  FillCompileOptionsForCachedStencil(options);

  // All XDR buffers are mmapped and live longer than JS runtime.
  // The bytecode can be borrowed from the buffer.
  options.borrowBuffer = true;
  options.usePinnedBytecode = true;

  JS::DecodeOptions decodeOptions(options);

  size_t decodingSourcesLength = decodingSources.length();

  if (!StaticPrefs::javascript_options_parallel_parsing() ||
      !StartDecodeTask(decodeOptions, std::move(decodingSources))) {
    LOG(Info, "Can't decode %lu bytes of scripts off-thread",
        (unsigned long)size);
    for (auto* script : mDecodingScripts) {
      script->mReadyToExecute = true;
    }
    return;
  }

  LOG(Debug, "Initialized decoding of %u scripts (%u bytes) in %fms\n",
      (unsigned)decodingSourcesLength, (unsigned)size,
      (TimeStamp::Now() - start).ToMilliseconds());
}

bool ScriptPreloader::StartDecodeTask(
    const JS::ReadOnlyDecodeOptions& decodeOptions,
    Vector<JS::TranscodeSource>&& decodingSources) {
  mDecodedStencils.emplace(decodingSources.length());
  MOZ_ASSERT(mDecodedStencils);

  nsCOMPtr<nsIRunnable> task =
      new DecodeTask(this, decodeOptions, std::move(decodingSources));

  nsresult rv = NS_DispatchBackgroundTask(task.forget());

  return NS_SUCCEEDED(rv);
}

NS_IMETHODIMP ScriptPreloader::DecodeTask::Run() {
  auto failure = [&]() {
    RefPtr<JS::Stencil> stencil;
    DebugOnly<int> writes = mPreloader->mDecodedStencils->Enqueue(stencil);
    MOZ_ASSERT(writes == 1);
    mPreloader->OnDecodeTaskFailed();
  };

  JS::FrontendContext* fc = JS::NewFrontendContext();
  if (!fc) {
    failure();
    return NS_OK;
  }

  auto cleanup = MakeScopeExit([&]() { JS::DestroyFrontendContext(fc); });

  size_t stackSize = TaskController::GetThreadStackSize();
  JS::SetNativeStackQuota(fc, JS::ThreadStackQuotaForSize(stackSize));

  size_t remaining = mDecodingSources.length();
  for (auto& source : mDecodingSources) {
    RefPtr<JS::Stencil> stencil;
    auto result = JS::DecodeStencil(fc, mDecodeOptions, source.range,
                                    getter_AddRefs(stencil));
    if (result != JS::TranscodeResult::Ok) {
      failure();
      return NS_OK;
    }

    DebugOnly<int> writes = mPreloader->mDecodedStencils->Enqueue(stencil);
    MOZ_ASSERT(writes == 1);

    remaining--;
    if (remaining) {
      mPreloader->onDecodedStencilQueued();
    }
  }

  mPreloader->OnDecodeTaskFinished();
  return NS_OK;
}

ScriptPreloader::CachedStencil::CachedStencil(ScriptPreloader& cache,
                                              InputBuffer& buf)
    : mCache(cache) {
  Code(buf);

  // Swap the mProcessTypes and mOriginalProcessTypes values, since we want to
  // start with an empty set of processes loaded into for this session, and
  // compare against last session's values later.
  mOriginalProcessTypes = mProcessTypes;
  mProcessTypes = {};
}

bool ScriptPreloader::CachedStencil::XDREncode(JSContext* cx) {
  auto cleanup = MakeScopeExit([&]() { MaybeDropStencil(); });

  mXDRData.construct<JS::TranscodeBuffer>();

  JS::TranscodeResult code = JS::EncodeStencil(cx, mStencil, Buffer());
  if (code == JS::TranscodeResult::Ok) {
    mXDRRange.emplace(Buffer().begin(), Buffer().length());
    mSize = Range().length();
    return true;
  }
  mXDRData.destroy();
  JS_ClearPendingException(cx);
  return false;
}

already_AddRefed<JS::Stencil> ScriptPreloader::CachedStencil::GetStencil(
    JSContext* cx, const JS::ReadOnlyDecodeOptions& options) {
  MOZ_ASSERT(mReadyToExecute);
  if (mStencil) {
    return do_AddRef(mStencil);
  }

  if (!HasRange()) {
    // We've already executed the script, and thrown it away. But it wasn't
    // in the cache at startup, so we don't have any data to decode. Give
    // up.
    return nullptr;
  }

  // If we have no script at this point, the script was too small to decode
  // off-thread, or it was needed before the off-thread compilation was
  // finished, and is small enough to decode on the main thread rather than
  // wait for the off-thread decoding to finish. In either case, we decode
  // it synchronously the first time it's needed.

  auto start = TimeStamp::Now();
  LOG(Info, "Decoding stencil %s on main thread...\n", mURL.get());

  RefPtr<JS::Stencil> stencil;
  if (JS::DecodeStencil(cx, options, Range(), getter_AddRefs(stencil)) ==
      JS::TranscodeResult::Ok) {
    // Lock the monitor here to avoid data races on mScript
    // from other threads like the cache writing thread.
    //
    // It is possible that we could end up decoding the same
    // script twice, because DecodeScript isn't being guarded
    // by the monitor; however, to encourage off-thread decode
    // to proceed for other scripts we don't hold the monitor
    // while doing main thread decode, merely while updating
    // mScript.
    mCache.mMonitor.AssertNotCurrentThreadOwns();
    MonitorAutoLock mal(mCache.mMonitor);

    mStencil = stencil.forget();

    if (mCache.mSaveComplete) {
      // We can only free XDR data if the stencil isn't borrowing data out of
      // it.
      if (!JS::StencilIsBorrowed(mStencil)) {
        FreeData();
      }
    }
  }

  LOG(Debug, "Finished decoding in %fms",
      (TimeStamp::Now() - start).ToMilliseconds());

  return do_AddRef(mStencil);
}

// nsIAsyncShutdownBlocker

nsresult ScriptPreloader::GetName(nsAString& aName) {
  aName.AssignLiteral(u"ScriptPreloader: Saving bytecode cache");
  return NS_OK;
}

nsresult ScriptPreloader::GetState(nsIPropertyBag** aState) {
  *aState = nullptr;
  return NS_OK;
}

nsresult ScriptPreloader::BlockShutdown(
    nsIAsyncShutdownClient* aBarrierClient) {
  // If we're waiting on a timeout to finish saving, interrupt it and just save
  // immediately.
  mSaveMonitor.NotifyAll();
  return NS_OK;
}

already_AddRefed<nsIAsyncShutdownClient> ScriptPreloader::GetShutdownBarrier() {
  nsCOMPtr<nsIAsyncShutdownService> svc = components::AsyncShutdown::Service();
  MOZ_RELEASE_ASSERT(svc);

  nsCOMPtr<nsIAsyncShutdownClient> barrier;
  Unused << svc->GetXpcomWillShutdown(getter_AddRefs(barrier));
  MOZ_RELEASE_ASSERT(barrier);

  return barrier.forget();
}

NS_IMPL_ISUPPORTS(ScriptPreloader, nsIObserver, nsIRunnable, nsIMemoryReporter,
                  nsINamed, nsIAsyncShutdownBlocker)

#undef LOG

}  // namespace mozilla
