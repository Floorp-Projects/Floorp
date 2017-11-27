/* -*-  Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef URLPreloader_h
#define URLPreloader_h

#include "mozilla/FileLocation.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Monitor.h"
#include "mozilla/Omnijar.h"
#include "mozilla/Range.h"
#include "mozilla/Vector.h"
#include "mozilla/Result.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsIChromeRegistry.h"
#include "nsIFile.h"
#include "nsIURI.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsIResProtocolHandler.h"
#include "nsIThread.h"
#include "nsReadableUtils.h"

class nsZipArchive;

namespace mozilla {
namespace loader {
    class InputBuffer;
}

using namespace mozilla::loader;

class ScriptPreloader;

/**
 * A singleton class to manage loading local URLs during startup, recording
 * them, and pre-loading them during early startup in the next session. URLs
 * that are not already loaded (or already being pre-loaded) when required are
 * read synchronously from disk, and (if startup is not already complete)
 * added to the pre-load list for the next session.
 */
class URLPreloader final : public nsIObserver
                         , public nsIMemoryReporter
{
    MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

    URLPreloader();

public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_NSIMEMORYREPORTER

    static URLPreloader& GetSingleton();

    // The type of read operation to perform.
    enum ReadType
    {
        // Read the file and then immediately forget its data.
        Forget,
        // Read the file and retain its data for the next caller.
        Retain,
    };

    // Helpers to read the contents of files or JAR archive entries with various
    // representations. If the preloader has not yet been initialized, or the
    // given location is not supported by the cache, the entries will be read
    // synchronously, and not stored in the cache.
    static Result<const nsCString, nsresult> Read(FileLocation& location, ReadType readType = Forget);

    static Result<const nsCString, nsresult> ReadURI(nsIURI* uri, ReadType readType = Forget);

    static Result<const nsCString, nsresult> ReadFile(nsIFile* file, ReadType readType = Forget);

    static Result<const nsCString, nsresult> ReadZip(nsZipArchive* archive,
                                                     const nsACString& path,
                                                     ReadType readType = Forget);

private:
    struct CacheKey;

    Result<const nsCString, nsresult> ReadInternal(const CacheKey& key, ReadType readType);

    Result<const nsCString, nsresult> ReadURIInternal(nsIURI* uri, ReadType readType);

    Result<const nsCString, nsresult> ReadFileInternal(nsIFile* file, ReadType readType);

    static Result<const nsCString, nsresult> Read(const CacheKey& key, ReadType readType);

    static bool sInitialized;

    static mozilla::StaticRefPtr<URLPreloader> sSingleton;

protected:
    friend class AddonManagerStartup;
    friend class ScriptPreloader;

    virtual ~URLPreloader();

    Result<Ok, nsresult> WriteCache();

    static URLPreloader& ReInitialize();

    // Clear leftover entries after the cache has been written.
    void Cleanup();

    // Begins reading files off-thread, and ensures that initialization has
    // completed before leaving the current scope. The caller *must* ensure that
    // no code on the main thread access Omnijar, either directly or indirectly,
    // for the lifetime of this guard object.
    struct MOZ_RAII AutoBeginReading final
    {
        AutoBeginReading()
        {
            GetSingleton().BeginBackgroundRead();
        }

        ~AutoBeginReading()
        {
            auto& reader = GetSingleton();

            MonitorAutoLock mal(reader.mMonitor);

            while (!reader.mReaderInitialized && reader.sInitialized) {
                mal.Wait();
            }
        }
    };

private:
    // Represents a key for an entry in the URI cache, based on its file or JAR
    // location.
    struct CacheKey
    {
        // The type of the entry. TypeAppJar and TypeGREJar entries are in the
        // app-specific or toolkit Omnijar files, and are handled specially.
        // TypeFile entries are plain files in the filesystem.
        enum EntryType : uint8_t
        {
            TypeAppJar,
            TypeGREJar,
            TypeFile,
        };

        CacheKey() = default;
        CacheKey(const CacheKey& other) = default;

        CacheKey(EntryType type, const nsACString& path)
            : mType(type), mPath(path)
        {}

        explicit CacheKey(nsIFile* file)
          : mType(TypeFile)
        {
            nsString path;
            MOZ_ALWAYS_SUCCEEDS(file->GetPath(path));
            CopyUTF16toUTF8(path, mPath);
        }

        explicit inline CacheKey(InputBuffer& buffer);

        // Encodes or decodes the cache key for storage in a session cache file.
        template <typename Buffer>
        void Code(Buffer& buffer)
        {
            buffer.codeUint8(*reinterpret_cast<uint8_t*>(&mType));
            buffer.codeString(mPath);
        }

        uint32_t Hash() const
        {
            return HashGeneric(mType, HashString(mPath));
        }

        bool operator==(const CacheKey& other) const
        {
            return mType == other.mType && mPath == other.mPath;
        }

        // Returns the Omnijar type for this entry. This may *only* be called
        // for Omnijar entries.
        Omnijar::Type OmnijarType()
        {
            switch (mType) {
            case TypeAppJar:
                return Omnijar::APP;
            case TypeGREJar:
                return Omnijar::GRE;
            default:
                MOZ_CRASH("Unexpected entry type");
                return Omnijar::GRE;
            }
        }

        const char* TypeString()
        {
            switch (mType) {
            case TypeAppJar: return "AppJar";
            case TypeGREJar: return "GREJar";
            case TypeFile: return "File";
            }
            MOZ_ASSERT_UNREACHABLE("no such type");
            return "";
        }

        already_AddRefed<nsZipArchive> Archive()
        {
            return Omnijar::GetReader(OmnijarType());
        }

        Result<FileLocation, nsresult> ToFileLocation();

        EntryType mType = TypeFile;

        // The path of the entry. For Type*Jar entries, this is the path within
        // the Omnijar archive. For TypeFile entries, this is the full path to
        // the file.
        nsCString mPath{};
    };

    // Represents an entry in the URI cache.
    struct URLEntry final : public CacheKey
                          , public LinkedListElement<URLEntry>
    {
        MOZ_IMPLICIT URLEntry(const CacheKey& key)
            : CacheKey(key)
            , mData(VoidCString())
        {}

        explicit URLEntry(nsIFile* file)
          : CacheKey(file)
        {}

        // For use with nsTArray::Sort.
        //
        // Sorts entries by the time they were initially read during this
        // session.
        struct Comparator final
        {
            bool Equals(const URLEntry* a, const URLEntry* b) const
            {
              return a->mReadTime == b->mReadTime;
            }

            bool LessThan(const URLEntry* a, const URLEntry* b) const
            {
                return a->mReadTime < b->mReadTime;
            }
        };

        // Sets the first-used time of this file to the earlier of its current
        // first-use time or the given timestamp.
        void UpdateUsedTime(const TimeStamp& time = TimeStamp::Now())
        {
          if (!mReadTime || time < mReadTime) {
            mReadTime = time;
          }
        }

        Result<const nsCString, nsresult> Read();
        static Result<const nsCString, nsresult> ReadLocation(FileLocation& location);

        size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
        {
            return (mallocSizeOf(this) +
                    mPath.SizeOfExcludingThisEvenIfShared(mallocSizeOf) +
                    mData.SizeOfExcludingThisEvenIfShared(mallocSizeOf));
        }

        // Reads the contents of the file referenced by this entry, or wait for
        // an off-thread read operation to finish if it is currently pending,
        // and return the file's contents.
        Result<const nsCString, nsresult> ReadOrWait(ReadType readType);

        nsCString mData;

        TimeStamp mReadTime{};

        nsresult mResultCode = NS_OK;
    };

    // Resolves the given URI to a CacheKey, if the URI is cacheable.
    Result<CacheKey, nsresult> ResolveURI(nsIURI* uri);

    Result<Ok, nsresult> InitInternal();

    // Returns a file pointer to the (possibly nonexistent) cache file with the
    // given suffix.
    Result<nsCOMPtr<nsIFile>, nsresult> GetCacheFile(const nsAString& suffix);
    // Finds the correct cache file to use for this session.
    Result<nsCOMPtr<nsIFile>, nsresult> FindCacheFile();

    Result<Ok, nsresult> ReadCache(LinkedList<URLEntry>& pendingURLs);

    void BackgroundReadFiles();
    void BeginBackgroundRead();

    using HashType = nsClassHashtable<nsGenericHashKey<CacheKey>, URLEntry>;

    size_t ShallowSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);


    bool mStartupFinished = false;
    bool mReaderInitialized = false;

    // Only to be accessed from the cache write thread.
    bool mCacheWritten = false;

    // The prefix URLs for files in the GRE and App omni jar archives.
    nsCString mGREPrefix;
    nsCString mAppPrefix;

    nsCOMPtr<nsIResProtocolHandler> mResProto;
    nsCOMPtr<nsIChromeRegistry> mChromeReg;
    nsCOMPtr<nsIFile> mProfD;

    nsCOMPtr<nsIThread> mReaderThread;

    // A map of URL entries which have were either read this session, or read
    // from the last session's cache file.
    HashType mCachedURLs;

    Monitor mMonitor{"[URLPreloader::mMutex]"};
};

} // namespace mozilla

#endif // URLPreloader_h
