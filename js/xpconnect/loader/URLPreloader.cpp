/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptPreloader-inl.h"
#include "mozilla/URLPreloader.h"
#include "mozilla/loader/AutoMemMap.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/FileUtils.h"
#include "mozilla/IOBuffers.h"
#include "mozilla/Logging.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/Try.h"
#include "mozilla/Unused.h"
#include "mozilla/Vector.h"
#include "mozilla/scache/StartupCache.h"

#include "crc32c.h"
#include "MainThreadUtils.h"
#include "nsPrintfCString.h"
#include "nsDebug.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsNetUtil.h"
#include "nsPromiseFlatString.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "nsZipArchive.h"
#include "xpcpublic.h"

namespace mozilla {
namespace {
static LazyLogModule gURLLog("URLPreloader");

#define LOG(level, ...) MOZ_LOG(gURLLog, LogLevel::level, (__VA_ARGS__))

template <typename T>
bool StartsWith(const T& haystack, const T& needle) {
  return StringHead(haystack, needle.Length()) == needle;
}
}  // anonymous namespace

using namespace mozilla::loader;
using mozilla::scache::StartupCache;

nsresult URLPreloader::CollectReports(nsIHandleReportCallback* aHandleReport,
                                      nsISupports* aData, bool aAnonymize) {
  MOZ_COLLECT_REPORT("explicit/url-preloader/other", KIND_HEAP, UNITS_BYTES,
                     ShallowSizeOfIncludingThis(MallocSizeOf),
                     "Memory used by the URL preloader service itself.");

  for (const auto& elem : mCachedURLs.Values()) {
    nsAutoCString pathName;
    pathName.Append(elem->mPath);
    // The backslashes will automatically be replaced with slashes in
    // about:memory, without splitting each path component into a separate
    // branch in the memory report tree.
    pathName.ReplaceChar('/', '\\');

    nsPrintfCString path("explicit/url-preloader/cached-urls/%s/[%s]",
                         elem->TypeString(), pathName.get());

    aHandleReport->Callback(
        ""_ns, path, KIND_HEAP, UNITS_BYTES,
        elem->SizeOfIncludingThis(MallocSizeOf),
        nsLiteralCString("Memory used to hold cache data for files which "
                         "have been read or pre-loaded during this session."),
        aData);
  }

  return NS_OK;
}

// static
already_AddRefed<URLPreloader> URLPreloader::Create(bool* aInitialized) {
  // The static APIs like URLPreloader::Read work in the child process because
  // they fall back to a synchronous read. The actual preloader must be
  // explicitly initialized, and this should only be done in the parent.
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());

  RefPtr<URLPreloader> preloader = new URLPreloader();
  if (preloader->InitInternal().isOk()) {
    *aInitialized = true;
    RegisterWeakMemoryReporter(preloader);
  } else {
    *aInitialized = false;
  }

  return preloader.forget();
}

URLPreloader& URLPreloader::GetSingleton() {
  if (!sSingleton) {
    sSingleton = Create(&sInitialized);
    ClearOnShutdown(&sSingleton);
  }

  return *sSingleton;
}

bool URLPreloader::sInitialized = false;

StaticRefPtr<URLPreloader> URLPreloader::sSingleton;

URLPreloader::~URLPreloader() {
  if (sInitialized) {
    UnregisterWeakMemoryReporter(this);
    sInitialized = false;
  }
}

Result<Ok, nsresult> URLPreloader::InitInternal() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (Omnijar::HasOmnijar(Omnijar::GRE)) {
    MOZ_TRY(Omnijar::GetURIString(Omnijar::GRE, mGREPrefix));
  }
  if (Omnijar::HasOmnijar(Omnijar::APP)) {
    MOZ_TRY(Omnijar::GetURIString(Omnijar::APP, mAppPrefix));
  }

  nsresult rv;
  nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
  MOZ_TRY(rv);

  nsCOMPtr<nsIProtocolHandler> ph;
  MOZ_TRY(ios->GetProtocolHandler("resource", getter_AddRefs(ph)));

  mResProto = do_QueryInterface(ph, &rv);
  MOZ_TRY(rv);

  mChromeReg = services::GetChromeRegistry();
  if (!mChromeReg) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  MOZ_TRY(NS_GetSpecialDirectory("ProfLDS", getter_AddRefs(mProfD)));

  return Ok();
}

URLPreloader& URLPreloader::ReInitialize() {
  MOZ_ASSERT(sSingleton);
  sSingleton = nullptr;
  sSingleton = Create(&sInitialized);
  return *sSingleton;
}

Result<nsCOMPtr<nsIFile>, nsresult> URLPreloader::GetCacheFile(
    const nsAString& suffix) {
  if (!mProfD) {
    return Err(NS_ERROR_NOT_INITIALIZED);
  }

  nsCOMPtr<nsIFile> cacheFile;
  MOZ_TRY(mProfD->Clone(getter_AddRefs(cacheFile)));

  MOZ_TRY(cacheFile->AppendNative("startupCache"_ns));
  Unused << cacheFile->Create(nsIFile::DIRECTORY_TYPE, 0777);

  MOZ_TRY(cacheFile->Append(u"urlCache"_ns + suffix));

  return std::move(cacheFile);
}

static const uint8_t URL_MAGIC[] = "mozURLcachev003";

Result<nsCOMPtr<nsIFile>, nsresult> URLPreloader::FindCacheFile() {
  if (StartupCache::GetIgnoreDiskCache()) {
    return Err(NS_ERROR_ABORT);
  }

  nsCOMPtr<nsIFile> cacheFile;
  MOZ_TRY_VAR(cacheFile, GetCacheFile(u".bin"_ns));

  bool exists;
  MOZ_TRY(cacheFile->Exists(&exists));
  if (exists) {
    MOZ_TRY(cacheFile->MoveTo(nullptr, u"urlCache-current.bin"_ns));
  } else {
    MOZ_TRY(cacheFile->SetLeafName(u"urlCache-current.bin"_ns));
    MOZ_TRY(cacheFile->Exists(&exists));
    if (!exists) {
      return Err(NS_ERROR_FILE_NOT_FOUND);
    }
  }

  return std::move(cacheFile);
}

Result<Ok, nsresult> URLPreloader::WriteCache() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mStartupFinished);

  // The script preloader might call us a second time, if it has to re-write
  // its cache after a cache flush. We don't care about cache flushes, since
  // our cache doesn't store any file data, only paths. And we currently clear
  // our cached file list after the first write, which means that a second
  // write would (aside from breaking the invariant that we never touch
  // mCachedURLs off-main-thread after the first write, and trigger a data
  // race) mean we get no pre-loading on the next startup.
  if (mCacheWritten) {
    return Ok();
  }
  mCacheWritten = true;

  LOG(Debug, "Writing cache...");

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

    nsTArray<URLEntry*> entries;
    for (const auto& entry : mCachedURLs.Values()) {
      if (entry->mReadTime) {
        entries.AppendElement(entry.get());
      }
    }

    entries.Sort(URLEntry::Comparator());

    OutputBuffer buf;
    for (auto entry : entries) {
      entry->Code(buf);
    }

    uint8_t headerSize[4];
    LittleEndian::writeUint32(headerSize, buf.cursor());

    uint8_t crc[4];
    LittleEndian::writeUint32(crc, ComputeCrc32c(~0, buf.Get(), buf.cursor()));

    MOZ_TRY(Write(fd, URL_MAGIC, sizeof(URL_MAGIC)));
    MOZ_TRY(Write(fd, headerSize, sizeof(headerSize)));
    MOZ_TRY(Write(fd, crc, sizeof(crc)));
    MOZ_TRY(Write(fd, buf.Get(), buf.cursor()));
  }

  MOZ_TRY(cacheFile->MoveTo(nullptr, u"urlCache.bin"_ns));

  NS_DispatchToMainThread(
      NewRunnableMethod("URLPreloader::Cleanup", this, &URLPreloader::Cleanup));

  return Ok();
}

void URLPreloader::Cleanup() { mCachedURLs.Clear(); }

Result<Ok, nsresult> URLPreloader::ReadCache(
    LinkedList<URLEntry>& pendingURLs) {
  LOG(Debug, "Reading cache...");

  nsCOMPtr<nsIFile> cacheFile;
  MOZ_TRY_VAR(cacheFile, FindCacheFile());

  AutoMemMap cache;
  MOZ_TRY(cache.init(cacheFile));

  auto size = cache.size();

  uint32_t headerSize;
  uint32_t crc;
  if (size < sizeof(URL_MAGIC) + sizeof(headerSize) + sizeof(crc)) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  auto data = cache.get<uint8_t>();
  auto end = data + size;

  if (memcmp(URL_MAGIC, data.get(), sizeof(URL_MAGIC))) {
    return Err(NS_ERROR_UNEXPECTED);
  }
  data += sizeof(URL_MAGIC);

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
    mMonitor.AssertCurrentThreadOwns();

    auto cleanup = MakeScopeExit([&]() {
      while (auto* elem = pendingURLs.getFirst()) {
        elem->remove();
      }
      mCachedURLs.Clear();
    });

    Range<uint8_t> header(data, data + headerSize);
    data += headerSize;

    InputBuffer buf(header);
    while (!buf.finished()) {
      CacheKey key(buf);

      LOG(Debug, "Cached file: %s %s", key.TypeString(), key.mPath.get());

      // Don't bother doing anything else if the key didn't load correctly.
      // We're going to throw it out right away, and it is possible that this
      // leads to pendingURLs getting into a weird state.
      if (buf.error()) {
        return Err(NS_ERROR_UNEXPECTED);
      }

      auto entry = mCachedURLs.GetOrInsertNew(key, key);
      entry->mResultCode = NS_ERROR_NOT_INITIALIZED;

      if (entry->isInList()) {
#ifdef NIGHTLY_BUILD
        MOZ_DIAGNOSTIC_ASSERT(pendingURLs.contains(entry),
                              "Entry should be in pendingURLs");
        MOZ_DIAGNOSTIC_ASSERT(key.mPath.Length() > 0,
                              "Path should be non-empty");
        MOZ_DIAGNOSTIC_ASSERT(false, "Entry should be new and not in any list");
#endif
        return Err(NS_ERROR_UNEXPECTED);
      }

      pendingURLs.insertBack(entry);
    }

    MOZ_RELEASE_ASSERT(!buf.error(),
                       "We should have already bailed on an error");

    cleanup.release();
  }

  return Ok();
}

void URLPreloader::BackgroundReadFiles() {
  auto cleanup = MakeScopeExit([&]() {
    auto lock = mReaderThread.Lock();
    auto& readerThread = lock.ref();
    NS_DispatchToMainThread(NewRunnableMethod(
        "nsIThread::AsyncShutdown", readerThread, &nsIThread::AsyncShutdown));

    readerThread = nullptr;
  });

  Vector<nsZipCursor> cursors;
  LinkedList<URLEntry> pendingURLs;
  {
    MonitorAutoLock mal(mMonitor);

    if (ReadCache(pendingURLs).isErr()) {
      mReaderInitialized = true;
      mal.NotifyAll();
      return;
    }

    int numZipEntries = 0;
    for (auto entry : pendingURLs) {
      if (entry->mType != entry->TypeFile) {
        numZipEntries++;
      }
    }
    MOZ_RELEASE_ASSERT(cursors.reserve(numZipEntries));

    // Initialize the zip cursors for all files in Omnijar while the monitor
    // is locked. Omnijar is not threadsafe, so the caller of
    // AutoBeginReading guard must ensure that no code accesses Omnijar
    // until this segment is done. Once the cursors have been initialized,
    // the actual reading and decompression can safely be done off-thread,
    // as is the case for thread-retargeted jar: channels.
    for (auto entry : pendingURLs) {
      if (entry->mType == entry->TypeFile) {
        continue;
      }

      RefPtr<nsZipArchive> zip = entry->Archive();
      if (!zip) {
        MOZ_CRASH_UNSAFE_PRINTF(
            "Failed to get Omnijar %s archive for entry (path: \"%s\")",
            entry->TypeString(), entry->mPath.get());
      }

      auto item = zip->GetItem(entry->mPath.get());
      if (!item) {
        entry->mResultCode = NS_ERROR_FILE_NOT_FOUND;
        continue;
      }

      size_t size = item->RealSize();

      entry->mData.SetLength(size);
      auto data = entry->mData.BeginWriting();

      cursors.infallibleEmplaceBack(item, zip, reinterpret_cast<uint8_t*>(data),
                                    size, true);
    }

    mReaderInitialized = true;
    mal.NotifyAll();
  }

  // Loop over the entries, read the file's contents, store them in the
  // entry's mData pointer, and notify any waiting threads to check for
  // completion.
  uint32_t i = 0;
  for (auto entry : pendingURLs) {
    // If there is any other error code, the entry has already failed at
    // this point, so don't bother trying to read it again.
    if (entry->mResultCode != NS_ERROR_NOT_INITIALIZED) {
      continue;
    }

    nsresult rv = NS_OK;

    LOG(Debug, "Background reading %s file %s", entry->TypeString(),
        entry->mPath.get());

    if (entry->mType == entry->TypeFile) {
      auto result = entry->Read();
      if (result.isErr()) {
        rv = result.unwrapErr();
      }
    } else {
      auto& cursor = cursors[i++];

      uint32_t len;
      cursor.Copy(&len);
      if (len != entry->mData.Length()) {
        entry->mData.Truncate();
        rv = NS_ERROR_FAILURE;
      }
    }

    entry->mResultCode = rv;
    mMonitor.NotifyAll();
  }

  // We're done reading pending entries, so clear the list.
  pendingURLs.clear();
}

void URLPreloader::BeginBackgroundRead() {
  auto lock = mReaderThread.Lock();
  auto& readerThread = lock.ref();
  if (!readerThread && !mReaderInitialized && sInitialized) {
    nsresult rv;
    rv = NS_NewNamedThread("BGReadURLs", getter_AddRefs(readerThread));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    nsCOMPtr<nsIRunnable> runnable =
        NewRunnableMethod("URLPreloader::BackgroundReadFiles", this,
                          &URLPreloader::BackgroundReadFiles);
    rv = readerThread->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // If we can't launch the task, just destroy the thread
      readerThread = nullptr;
      return;
    }
  }
}

Result<nsCString, nsresult> URLPreloader::ReadInternal(const CacheKey& key,
                                                       ReadType readType) {
  if (mStartupFinished || !mReaderInitialized) {
    URLEntry entry(key);

    return entry.Read();
  }

  auto entry = mCachedURLs.GetOrInsertNew(key, key);

  entry->UpdateUsedTime();

  return entry->ReadOrWait(readType);
}

Result<nsCString, nsresult> URLPreloader::ReadURIInternal(nsIURI* uri,
                                                          ReadType readType) {
  CacheKey key;
  MOZ_TRY_VAR(key, ResolveURI(uri));

  return ReadInternal(key, readType);
}

/* static */ Result<nsCString, nsresult> URLPreloader::Read(const CacheKey& key,
                                                            ReadType readType) {
  // If we're being called before the preloader has been initialized (i.e.,
  // before the profile has been initialized), just fall back to a synchronous
  // read. This happens when we're reading .ini and preference files that are
  // needed to locate and initialize the profile.
  if (!sInitialized) {
    return URLEntry(key).Read();
  }

  return GetSingleton().ReadInternal(key, readType);
}

/* static */ Result<nsCString, nsresult> URLPreloader::ReadURI(
    nsIURI* uri, ReadType readType) {
  if (!sInitialized) {
    return Err(NS_ERROR_NOT_INITIALIZED);
  }

  return GetSingleton().ReadURIInternal(uri, readType);
}

/* static */ Result<nsCString, nsresult> URLPreloader::ReadFile(
    nsIFile* file, ReadType readType) {
  return Read(CacheKey(file), readType);
}

/* static */ Result<nsCString, nsresult> URLPreloader::Read(
    FileLocation& location, ReadType readType) {
  if (location.IsZip()) {
    if (location.GetBaseZip()) {
      nsCString path;
      location.GetPath(path);
      return ReadZip(location.GetBaseZip(), path);
    }
    return URLEntry::ReadLocation(location);
  }

  nsCOMPtr<nsIFile> file = location.GetBaseFile();
  return ReadFile(file, readType);
}

/* static */ Result<nsCString, nsresult> URLPreloader::ReadZip(
    nsZipArchive* zip, const nsACString& path, ReadType readType) {
  // If the zip archive belongs to an Omnijar location, map it to a cache
  // entry, and cache it as normal. Otherwise, simply read the entry
  // synchronously, since other JAR archives are currently unsupported by the
  // cache.
  RefPtr<nsZipArchive> reader = Omnijar::GetReader(Omnijar::GRE);
  if (zip == reader) {
    CacheKey key(CacheKey::TypeGREJar, path);
    return Read(key, readType);
  }

  reader = Omnijar::GetReader(Omnijar::APP);
  if (zip == reader) {
    CacheKey key(CacheKey::TypeAppJar, path);
    return Read(key, readType);
  }

  // Not an Omnijar archive, so just read it directly.
  FileLocation location(zip, PromiseFlatCString(path).BeginReading());
  return URLEntry::ReadLocation(location);
}

Result<URLPreloader::CacheKey, nsresult> URLPreloader::ResolveURI(nsIURI* uri) {
  nsCString spec;
  nsCString scheme;
  MOZ_TRY(uri->GetSpec(spec));
  MOZ_TRY(uri->GetScheme(scheme));

  nsCOMPtr<nsIURI> resolved;

  // If the URI is a resource: or chrome: URI, first resolve it to the
  // underlying URI that it wraps.
  if (scheme.EqualsLiteral("resource")) {
    MOZ_TRY(mResProto->ResolveURI(uri, spec));
    MOZ_TRY(NS_NewURI(getter_AddRefs(resolved), spec));
  } else if (scheme.EqualsLiteral("chrome")) {
    MOZ_TRY(mChromeReg->ConvertChromeURL(uri, getter_AddRefs(resolved)));
    MOZ_TRY(resolved->GetSpec(spec));
  } else {
    resolved = uri;
  }
  MOZ_TRY(resolved->GetScheme(scheme));

  // Try the GRE and App Omnijar prefixes.
  if (mGREPrefix.Length() && StartsWith(spec, mGREPrefix)) {
    return CacheKey(CacheKey::TypeGREJar, Substring(spec, mGREPrefix.Length()));
  }

  if (mAppPrefix.Length() && StartsWith(spec, mAppPrefix)) {
    return CacheKey(CacheKey::TypeAppJar, Substring(spec, mAppPrefix.Length()));
  }

  // Try for a file URI.
  if (scheme.EqualsLiteral("file")) {
    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(resolved);
    MOZ_ASSERT(fileURL);

    nsCOMPtr<nsIFile> file;
    MOZ_TRY(fileURL->GetFile(getter_AddRefs(file)));

    nsString path;
    MOZ_TRY(file->GetPath(path));

    return CacheKey(CacheKey::TypeFile, NS_ConvertUTF16toUTF8(path));
  }

  // Not a file or Omnijar URI, so currently unsupported.
  return Err(NS_ERROR_INVALID_ARG);
}

size_t URLPreloader::ShallowSizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) {
  return (mallocSizeOf(this) +
          mAppPrefix.SizeOfExcludingThisEvenIfShared(mallocSizeOf) +
          mGREPrefix.SizeOfExcludingThisEvenIfShared(mallocSizeOf) +
          mCachedURLs.ShallowSizeOfExcludingThis(mallocSizeOf));
}

Result<FileLocation, nsresult> URLPreloader::CacheKey::ToFileLocation() {
  if (mType == TypeFile) {
    nsCOMPtr<nsIFile> file;
    MOZ_TRY(NS_NewLocalFile(NS_ConvertUTF8toUTF16(mPath), false,
                            getter_AddRefs(file)));
    return FileLocation(file);
  }

  RefPtr<nsZipArchive> zip = Archive();
  return FileLocation(zip, mPath.get());
}

Result<nsCString, nsresult> URLPreloader::URLEntry::Read() {
  FileLocation location;
  MOZ_TRY_VAR(location, ToFileLocation());

  MOZ_TRY_VAR(mData, ReadLocation(location));
  return mData;
}

/* static */ Result<nsCString, nsresult> URLPreloader::URLEntry::ReadLocation(
    FileLocation& location) {
  FileLocation::Data data;
  MOZ_TRY(location.GetData(data));

  uint32_t size;
  MOZ_TRY(data.GetSize(&size));

  nsCString result;
  result.SetLength(size);
  MOZ_TRY(data.Copy(result.BeginWriting(), size));

  return std::move(result);
}

Result<nsCString, nsresult> URLPreloader::URLEntry::ReadOrWait(
    ReadType readType) {
  auto now = TimeStamp::Now();
  LOG(Info, "Reading %s\n", mPath.get());
  auto cleanup = MakeScopeExit([&]() {
    LOG(Info, "Read in %fms\n", (TimeStamp::Now() - now).ToMilliseconds());
  });

  if (mResultCode == NS_ERROR_NOT_INITIALIZED) {
    MonitorAutoLock mal(GetSingleton().mMonitor);

    while (mResultCode == NS_ERROR_NOT_INITIALIZED) {
      mal.Wait();
    }
  }

  if (mResultCode == NS_OK && mData.IsVoid()) {
    LOG(Info, "Reading synchronously...\n");
    return Read();
  }

  if (NS_FAILED(mResultCode)) {
    return Err(mResultCode);
  }

  nsCString res = mData;

  if (readType == Forget) {
    mData.SetIsVoid(true);
  }
  return res;
}

inline URLPreloader::CacheKey::CacheKey(InputBuffer& buffer) {
  Code(buffer);
  MOZ_DIAGNOSTIC_ASSERT(
      mType == TypeAppJar || mType == TypeGREJar || mType == TypeFile,
      "mType should be valid");
}

NS_IMPL_ISUPPORTS(URLPreloader, nsIMemoryReporter)

#undef LOG

}  // namespace mozilla
