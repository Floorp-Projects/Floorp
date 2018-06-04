/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptPreloader-inl.h"
#include "mozilla/URLPreloader.h"
#include "mozilla/loader/AutoMemMap.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "mozilla/Vector.h"

#include "MainThreadUtils.h"
#include "nsPrintfCString.h"
#include "nsDebug.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIObserverService.h"
#include "nsNetUtil.h"
#include "nsPromiseFlatString.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "nsZipArchive.h"
#include "xpcpublic.h"

#include "mozilla/dom/ContentChild.h"

#undef DELAYED_STARTUP_TOPIC
#define DELAYED_STARTUP_TOPIC "sessionstore-windows-restored"

namespace mozilla {
namespace {
static LazyLogModule gURLLog("URLPreloader");

#define LOG(level, ...) MOZ_LOG(gURLLog, LogLevel::level, (__VA_ARGS__))

template<typename T>
bool
StartsWith(const T& haystack, const T& needle)
{
    return StringHead(haystack, needle.Length()) == needle;
}
} // anonymous namespace

using namespace mozilla::loader;

nsresult
URLPreloader::CollectReports(nsIHandleReportCallback* aHandleReport,
                             nsISupports* aData, bool aAnonymize)
{
    MOZ_COLLECT_REPORT(
        "explicit/url-preloader/other", KIND_HEAP, UNITS_BYTES,
        ShallowSizeOfIncludingThis(MallocSizeOf),
        "Memory used by the URL preloader service itself.");

    for (const auto& elem : IterHash(mCachedURLs)) {
        nsAutoCString pathName;
        pathName.Append(elem->mPath);
        // The backslashes will automatically be replaced with slashes in
        // about:memory, without splitting each path component into a separate
        // branch in the memory report tree.
        pathName.ReplaceChar('/', '\\');

        nsPrintfCString path("explicit/url-preloader/cached-urls/%s/[%s]",
                             elem->TypeString(), pathName.get());

        aHandleReport->Callback(
            EmptyCString(), path, KIND_HEAP, UNITS_BYTES,
            elem->SizeOfIncludingThis(MallocSizeOf),
            NS_LITERAL_CSTRING("Memory used to hold cache data for files which "
                               "have been read or pre-loaded during this session."),
            aData);
    }

    return NS_OK;
}

URLPreloader&
URLPreloader::GetSingleton()
{
    if (!sSingleton) {
        sSingleton = new URLPreloader();
        ClearOnShutdown(&sSingleton);
    }

    return *sSingleton;
}

bool URLPreloader::sInitialized = false;

StaticRefPtr<URLPreloader> URLPreloader::sSingleton;

URLPreloader::URLPreloader()
{
    if (InitInternal().isOk()) {
        sInitialized = true;
        RegisterWeakMemoryReporter(this);
    }
}

URLPreloader::~URLPreloader()
{
    if (sInitialized) {
        UnregisterWeakMemoryReporter(this);
    }
}

Result<Ok, nsresult>
URLPreloader::InitInternal()
{
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

    mChromeReg = services::GetChromeRegistryService();
    if (!mChromeReg) {
        return Err(NS_ERROR_UNEXPECTED);
    }

    if (XRE_IsParentProcess()) {
        nsCOMPtr<nsIObserverService> obs = services::GetObserverService();

        obs->AddObserver(this, DELAYED_STARTUP_TOPIC, false);

        MOZ_TRY(NS_GetSpecialDirectory("ProfLDS", getter_AddRefs(mProfD)));
    } else {
        mStartupFinished = true;
        mReaderInitialized = true;
    }

    return Ok();
}

URLPreloader&
URLPreloader::ReInitialize()
{
    sSingleton = new URLPreloader();

    return *sSingleton;
}

Result<nsCOMPtr<nsIFile>, nsresult>
URLPreloader::GetCacheFile(const nsAString& suffix)
{
    if (!mProfD) {
        return Err(NS_ERROR_NOT_INITIALIZED);
    }

    nsCOMPtr<nsIFile> cacheFile;
    MOZ_TRY(mProfD->Clone(getter_AddRefs(cacheFile)));

    MOZ_TRY(cacheFile->AppendNative(NS_LITERAL_CSTRING("startupCache")));
    Unused << cacheFile->Create(nsIFile::DIRECTORY_TYPE, 0777);

    MOZ_TRY(cacheFile->Append(NS_LITERAL_STRING("urlCache") + suffix));

    return std::move(cacheFile);
}

static const uint8_t URL_MAGIC[] = "mozURLcachev002";

Result<nsCOMPtr<nsIFile>, nsresult>
URLPreloader::FindCacheFile()
{
    nsCOMPtr<nsIFile> cacheFile;
    MOZ_TRY_VAR(cacheFile, GetCacheFile(NS_LITERAL_STRING(".bin")));

    bool exists;
    MOZ_TRY(cacheFile->Exists(&exists));
    if (exists) {
        MOZ_TRY(cacheFile->MoveTo(nullptr, NS_LITERAL_STRING("urlCache-current.bin")));
    } else {
        MOZ_TRY(cacheFile->SetLeafName(NS_LITERAL_STRING("urlCache-current.bin")));
        MOZ_TRY(cacheFile->Exists(&exists));
        if (!exists) {
            return Err(NS_ERROR_FILE_NOT_FOUND);
        }
    }

    return std::move(cacheFile);
}

Result<Ok, nsresult>
URLPreloader::WriteCache()
{
    MOZ_ASSERT(!NS_IsMainThread());

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
    MOZ_TRY_VAR(cacheFile, GetCacheFile(NS_LITERAL_STRING("-new.bin")));

    bool exists;
    MOZ_TRY(cacheFile->Exists(&exists));
    if (exists) {
        MOZ_TRY(cacheFile->Remove(false));
    }

    {
        AutoFDClose fd;
        MOZ_TRY(cacheFile->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE, 0644, &fd.rwget()));

        nsTArray<URLEntry*> entries;
        for (auto& entry : IterHash(mCachedURLs)) {
            if (entry->mReadTime) {
                entries.AppendElement(entry);
            }
        }

        entries.Sort(URLEntry::Comparator());

        OutputBuffer buf;
        for (auto entry : entries) {
            entry->Code(buf);
        }

        uint8_t headerSize[4];
        LittleEndian::writeUint32(headerSize, buf.cursor());

        MOZ_TRY(Write(fd, URL_MAGIC, sizeof(URL_MAGIC)));
        MOZ_TRY(Write(fd, headerSize, sizeof(headerSize)));
        MOZ_TRY(Write(fd, buf.Get(), buf.cursor()));
    }

    MOZ_TRY(cacheFile->MoveTo(nullptr, NS_LITERAL_STRING("urlCache.bin")));

    NS_DispatchToMainThread(
        NewRunnableMethod("URLPreloader::Cleanup",
                          this,
                          &URLPreloader::Cleanup));

    return Ok();
}

void
URLPreloader::Cleanup()
{
    mCachedURLs.Clear();
}

Result<Ok, nsresult>
URLPreloader::ReadCache(LinkedList<URLEntry>& pendingURLs)
{
    LOG(Debug, "Reading cache...");

    nsCOMPtr<nsIFile> cacheFile;
    MOZ_TRY_VAR(cacheFile, FindCacheFile());

    AutoMemMap cache;
    MOZ_TRY(cache.init(cacheFile));

    auto size = cache.size();

    uint32_t headerSize;
    if (size < sizeof(URL_MAGIC) + sizeof(headerSize)) {
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

    if (data + headerSize > end) {
        return Err(NS_ERROR_UNEXPECTED);
    }

    {
        mMonitor.AssertCurrentThreadOwns();

        auto cleanup = MakeScopeExit([&] () {
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

            auto entry = mCachedURLs.LookupOrAdd(key, key);
            entry->mResultCode = NS_ERROR_NOT_INITIALIZED;

            pendingURLs.insertBack(entry);
        }

        if (buf.error()) {
            return Err(NS_ERROR_UNEXPECTED);
        }

        cleanup.release();
    }

    return Ok();
}

void
URLPreloader::BackgroundReadFiles()
{
    auto cleanup = MakeScopeExit([&] () {
        NS_DispatchToMainThread(
            NewRunnableMethod("nsIThread::Shutdown",
                              mReaderThread, &nsIThread::Shutdown));
        mReaderThread = nullptr;
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

        LOG(Debug, "Background reading %s file %s", entry->TypeString(), entry->mPath.get());

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

void
URLPreloader::BeginBackgroundRead()
{
    if (!mReaderThread && !mReaderInitialized && sInitialized) {
        nsCOMPtr<nsIRunnable> runnable =
            NewRunnableMethod("URLPreloader::BackgroundReadFiles",
                              this,
                              &URLPreloader::BackgroundReadFiles);

        Unused << NS_NewNamedThread(
            "BGReadURLs", getter_AddRefs(mReaderThread), runnable);
    }
}


Result<const nsCString, nsresult>
URLPreloader::ReadInternal(const CacheKey& key, ReadType readType)
{
    if (mStartupFinished) {
        URLEntry entry(key);

        return entry.Read();
    }

    auto entry = mCachedURLs.LookupOrAdd(key, key);

    entry->UpdateUsedTime();

    return entry->ReadOrWait(readType);
}

Result<const nsCString, nsresult>
URLPreloader::ReadURIInternal(nsIURI* uri, ReadType readType)
{
    CacheKey key;
    MOZ_TRY_VAR(key, ResolveURI(uri));

    return ReadInternal(key, readType);
}

/* static */ Result<const nsCString, nsresult>
URLPreloader::Read(const CacheKey& key, ReadType readType)
{
    // If we're being called before the preloader has been initialized (i.e.,
    // before the profile has been initialized), just fall back to a synchronous
    // read. This happens when we're reading .ini and preference files that are
    // needed to locate and initialize the profile.
    if (!sInitialized) {
        return URLEntry(key).Read();
    }

    return GetSingleton().ReadInternal(key, readType);
}

/* static */ Result<const nsCString, nsresult>
URLPreloader::ReadURI(nsIURI* uri, ReadType readType)
{
    if (!sInitialized) {
        return Err(NS_ERROR_NOT_INITIALIZED);
    }

    return GetSingleton().ReadURIInternal(uri, readType);
}

/* static */ Result<const nsCString, nsresult>
URLPreloader::ReadFile(nsIFile* file, ReadType readType)
{
    return Read(CacheKey(file), readType);
}

/* static */ Result<const nsCString, nsresult>
URLPreloader::Read(FileLocation& location, ReadType readType)
{
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

/* static */ Result<const nsCString, nsresult>
URLPreloader::ReadZip(nsZipArchive* zip, const nsACString& path, ReadType readType)
{
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

Result<URLPreloader::CacheKey, nsresult>
URLPreloader::ResolveURI(nsIURI* uri)
{
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
        return CacheKey(CacheKey::TypeGREJar,
                        Substring(spec, mGREPrefix.Length()));
    }

    if (mAppPrefix.Length() && StartsWith(spec, mAppPrefix)) {
        return CacheKey(CacheKey::TypeAppJar,
                        Substring(spec, mAppPrefix.Length()));
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

size_t
URLPreloader::ShallowSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    return (mallocSizeOf(this) +
            mAppPrefix.SizeOfExcludingThisEvenIfShared(mallocSizeOf) +
            mGREPrefix.SizeOfExcludingThisEvenIfShared(mallocSizeOf) +
            mCachedURLs.ShallowSizeOfExcludingThis(mallocSizeOf));
}

Result<FileLocation, nsresult>
URLPreloader::CacheKey::ToFileLocation()
{
    if (mType == TypeFile) {
        nsCOMPtr<nsIFile> file;
        MOZ_TRY(NS_NewLocalFile(NS_ConvertUTF8toUTF16(mPath), false,
                                getter_AddRefs(file)));
        return FileLocation(file);
    }

    RefPtr<nsZipArchive> zip = Archive();
    return FileLocation(zip, mPath.get());
}

Result<const nsCString, nsresult>
URLPreloader::URLEntry::Read()
{
    FileLocation location;
    MOZ_TRY_VAR(location, ToFileLocation());

    MOZ_TRY_VAR(mData, ReadLocation(location));
    return mData;
}

/* static */ Result<const nsCString, nsresult>
URLPreloader::URLEntry::ReadLocation(FileLocation& location)
{
    FileLocation::Data data;
    MOZ_TRY(location.GetData(data));

    uint32_t size;
    MOZ_TRY(data.GetSize(&size));

    nsCString result;
    result.SetLength(size);
    MOZ_TRY(data.Copy(result.BeginWriting(), size));

    return std::move(result);
}

Result<const nsCString, nsresult>
URLPreloader::URLEntry::ReadOrWait(ReadType readType)
{
    auto now = TimeStamp::Now();
    LOG(Info, "Reading %s\n", mPath.get());
    auto cleanup = MakeScopeExit([&] () {
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

inline
URLPreloader::CacheKey::CacheKey(InputBuffer& buffer)
{
    Code(buffer);
}

nsresult
URLPreloader::Observe(nsISupports* subject, const char* topic, const char16_t* data)
{
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (!strcmp(topic, DELAYED_STARTUP_TOPIC)) {
        obs->RemoveObserver(this, DELAYED_STARTUP_TOPIC);
        mStartupFinished = true;
    }

    return NS_OK;
}


NS_IMPL_ISUPPORTS(URLPreloader, nsIObserver, nsIMemoryReporter)

#undef LOG

} // namespace mozilla
