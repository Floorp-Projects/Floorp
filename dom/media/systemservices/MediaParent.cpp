/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaParent.h"

#include "mozilla/Base64.h"
#include <mozilla/StaticMutex.h>

#include "MediaUtils.h"
#include "MediaEngine.h"
#include "VideoUtils.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsILineInputStream.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsISupportsImpl.h"
#include "mozilla/Logging.h"

#undef LOG
PRLogModuleInfo *gMediaParentLog;
#define LOG(args) MOZ_LOG(gMediaParentLog, mozilla::LogLevel::Debug, args)

// A file in the profile dir is used to persist mOriginKeys used to anonymize
// deviceIds to be unique per origin, to avoid them being supercookies.

#define ORIGINKEYS_FILE "enumerate_devices.txt"
#define ORIGINKEYS_VERSION "1"

namespace mozilla {
namespace media {

static StaticMutex gMutex;
static ParentSingleton* sParentSingleton = nullptr;

class ParentSingleton : public nsISupports
{
  NS_DECL_THREADSAFE_ISUPPORTS

  class OriginKey
  {
  public:
    static const size_t DecodedLength = 18;
    static const size_t EncodedLength = DecodedLength * 4 / 3;

    OriginKey(const nsACString& aKey, int64_t aSecondsStamp)
    : mKey(aKey)
    , mSecondsStamp(aSecondsStamp) {}

    nsCString mKey; // Base64 encoded.
    int64_t mSecondsStamp;
  };

  class OriginKeysTable
  {
  public:
    OriginKeysTable() {}

    nsresult
    GetOriginKey(const nsACString& aOrigin, nsCString& result)
    {
      OriginKey* key;
      if (!mKeys.Get(aOrigin, &key)) {
        nsCString salt; // Make a new one
        nsresult rv = GenerateRandomName(salt, key->EncodedLength);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        key = new OriginKey(salt, PR_Now() / PR_USEC_PER_SEC);
        mKeys.Put(aOrigin, key);
      }
      result = key->mKey;
      return NS_OK;
    }

    static PLDHashOperator
    HashCleaner(const nsACString& aOrigin, nsAutoPtr<OriginKey>& aOriginKey,
                void *aUserArg)
    {
      OriginKey* since = static_cast<OriginKey*>(aUserArg);

      LOG((((aOriginKey->mSecondsStamp >= since->mSecondsStamp)?
            "%s: REMOVE %lld >= %lld" :
            "%s: KEEP   %lld < %lld"),
            __FUNCTION__, aOriginKey->mSecondsStamp, since->mSecondsStamp));

      return (aOriginKey->mSecondsStamp >= since->mSecondsStamp)?
          PL_DHASH_REMOVE : PL_DHASH_NEXT;
    }

    void Clear(int64_t aSinceWhen)
    {
      // Avoid int64_t* <-> void* casting offset
      OriginKey since(nsCString(), aSinceWhen  / PR_USEC_PER_SEC);
      mKeys.Enumerate(HashCleaner, &since);
    }

  protected:
    nsClassHashtable<nsCStringHashKey, OriginKey> mKeys;
  };

  class OriginKeysLoader : public OriginKeysTable
  {
  public:
    OriginKeysLoader() {}

    nsresult
    GetOriginKey(const nsACString& aOrigin, nsCString& result)
    {
      auto before = mKeys.Count();
      OriginKeysTable::GetOriginKey(aOrigin, result);
      if (mKeys.Count() != before) {
        Save();
      }
      return NS_OK;
    }

    already_AddRefed<nsIFile>
    GetFile()
    {
      MOZ_ASSERT(mProfileDir);
      nsCOMPtr<nsIFile> file;
      nsresult rv = mProfileDir->Clone(getter_AddRefs(file));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }
      file->Append(NS_LITERAL_STRING(ORIGINKEYS_FILE));
      return file.forget();
    }

    // Format of file is key secondsstamp origin (first line is version #):
    //
    // 1
    // rOMAAbFujNwKyIpj4RJ3Wt5Q 1424733961 http://fiddle.jshell.net
    // rOMAAbFujNwKyIpj4RJ3Wt5Q 1424734841 http://mozilla.github.io
    // etc.

    nsresult Read()
    {
      nsCOMPtr<nsIFile> file = GetFile();
      if (NS_WARN_IF(!file)) {
        return NS_ERROR_UNEXPECTED;
      }
      bool exists;
      nsresult rv = file->Exists(&exists);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (!exists) {
        return NS_OK;
      }

      nsCOMPtr<nsIInputStream> stream;
      rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), file);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      nsCOMPtr<nsILineInputStream> i = do_QueryInterface(stream);
      MOZ_ASSERT(i);

      nsCString line;
      bool hasMoreLines;
      rv = i->ReadLine(line, &hasMoreLines);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (!line.EqualsLiteral(ORIGINKEYS_VERSION)) {
        // If version on disk is newer than we can understand then ignore it.
        return NS_OK;
      }

      while (hasMoreLines) {
        rv = i->ReadLine(line, &hasMoreLines);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        // Read key secondsstamp origin.
        // Ignore any lines that don't fit format in the comment above exactly.
        int32_t f = line.FindChar(' ');
        if (f < 0) {
          continue;
        }
        const nsACString& key = Substring(line, 0, f);
        const nsACString& s = Substring(line, f+1);
        f = s.FindChar(' ');
        if (f < 0) {
          continue;
        }
        int64_t secondsstamp = nsCString(Substring(s, 0, f)).ToInteger64(&rv);
        if (NS_FAILED(rv)) {
          continue;
        }
        const nsACString& origin = Substring(s, f+1);

        // Validate key
        if (key.Length() != OriginKey::EncodedLength) {
          continue;
        }
        nsCString dummy;
        rv = Base64Decode(key, dummy);
        if (NS_FAILED(rv)) {
          continue;
        }
        mKeys.Put(origin, new OriginKey(key, secondsstamp));
      }
      return NS_OK;
    }

    static PLDHashOperator
    HashWriter(const nsACString& aOrigin, OriginKey* aOriginKey, void *aUserArg)
    {
      auto* stream = static_cast<nsIOutputStream *>(aUserArg);

      nsCString buffer;
      buffer.Append(aOriginKey->mKey);
      buffer.Append(' ');
      buffer.AppendInt(aOriginKey->mSecondsStamp);
      buffer.Append(' ');
      buffer.Append(aOrigin);
      buffer.Append('\n');

      uint32_t count;
      nsresult rv = stream->Write(buffer.Data(), buffer.Length(), &count);
      if (NS_WARN_IF(NS_FAILED(rv)) || count != buffer.Length()) {
        return PL_DHASH_STOP;
      }
      return PL_DHASH_NEXT;
    }

    nsresult
    Write()
    {
      nsCOMPtr<nsIFile> file = GetFile();
      if (NS_WARN_IF(!file)) {
        return NS_ERROR_UNEXPECTED;
      }

      nsCOMPtr<nsIOutputStream> stream;
      nsresult rv = NS_NewSafeLocalFileOutputStream(getter_AddRefs(stream), file);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      nsAutoCString buffer;
      buffer.AppendLiteral(ORIGINKEYS_VERSION);
      buffer.Append('\n');

      uint32_t count;
      rv = stream->Write(buffer.Data(), buffer.Length(), &count);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (count != buffer.Length()) {
        return NS_ERROR_UNEXPECTED;
      }
      mKeys.EnumerateRead(HashWriter, stream.get());

      nsCOMPtr<nsISafeOutputStream> safeStream = do_QueryInterface(stream);
      MOZ_ASSERT(safeStream);

      rv = safeStream->Finish();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }

    nsresult Load()
    {
      nsresult rv = Read();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        Delete();
      }
      return rv;
    }

    nsresult Save()
    {
      nsresult rv = Write();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        NS_WARNING("Failed to write data for EnumerateDevices id-persistence.");
        Delete();
      }
      return rv;
    }

    void Clear(int64_t aSinceWhen)
    {
      OriginKeysTable::Clear(aSinceWhen);
      Delete();
      Save();
    }

    nsresult Delete()
    {
      nsCOMPtr<nsIFile> file = GetFile();
      if (NS_WARN_IF(!file)) {
        return NS_ERROR_UNEXPECTED;
      }
      nsresult rv = file->Remove(false);
      if (rv == NS_ERROR_FILE_NOT_FOUND) {
        return NS_OK;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }

    void
    SetProfileDir(nsIFile* aProfileDir)
    {
      MOZ_ASSERT(!NS_IsMainThread());
      bool first = !mProfileDir;
      mProfileDir = aProfileDir;
      // Load from disk when we first get a profileDir, but not subsequently.
      if (first) {
        Load();
      }
    }
  private:
    nsCOMPtr<nsIFile> mProfileDir;
  };

private:
  virtual ~ParentSingleton()
  {
    sParentSingleton = nullptr;
    LOG((__FUNCTION__));
  }

public:
  static ParentSingleton* Get()
  {
    // Protect creation of singleton and access from multiple Background threads.
    //
    // Multiple Background threads happen because sanitize.js calls us from the
    // chrome process and gets a thread separate from the one servicing ipc from
    // the content process.

    StaticMutexAutoLock lock(gMutex);
    if (!sParentSingleton) {
      sParentSingleton = new ParentSingleton();
    }
    return sParentSingleton;
  }

  // Only accessed on StreamTS thread
  OriginKeysLoader mOriginKeys;
  OriginKeysTable mPrivateBrowsingOriginKeys;

  // Only accessed on return thread
  CoatCheck<Pledge<nsCString>> mOutstandingPledges;
};

NS_IMPL_ISUPPORTS0(ParentSingleton)

bool
Parent::RecvGetOriginKey(const uint32_t& aRequestId,
                         const nsCString& aOrigin,
                         const bool& aPrivateBrowsing)
{
  // TODO: Replace all this when moving MediaParent to PContent soon (1037389)

  nsRefPtr<ParentSingleton> singleton(mSingleton);
  nsCOMPtr<nsIThread> returnThread = NS_GetCurrentThread();
  nsRefPtr<Pledge<nsCString>> p = new Pledge<nsCString>();
  nsresult rv;

  // First, over to main thread to get profile dir.

  // Pledges are non-threadsafe by design, so check them and pass an id instead.
  uint32_t id = singleton->mOutstandingPledges.Append(*p);

  rv = NS_DispatchToMainThread(NewRunnableFrom([id, returnThread,
                                                singleton, aOrigin,
                                                aPrivateBrowsing]() -> nsresult {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIFile> profileDir;
    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                         getter_AddRefs(profileDir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Then from there over to stream-transport thread to do the actual file io.

    nsCOMPtr<nsIEventTarget> sts = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    MOZ_ASSERT(sts);
    rv = sts->Dispatch(NewRunnableFrom([profileDir, id, returnThread, singleton,
                                        aOrigin, aPrivateBrowsing]() -> nsresult {
      MOZ_ASSERT(!NS_IsMainThread());
      singleton->mOriginKeys.SetProfileDir(profileDir);
      nsCString result;
      if (aPrivateBrowsing) {
        singleton->mPrivateBrowsingOriginKeys.GetOriginKey(aOrigin, result);
      } else {
        singleton->mOriginKeys.GetOriginKey(aOrigin, result);
      }

      // Pass result back to original thread.
      nsresult rv;
      rv = returnThread->Dispatch(NewRunnableFrom([id, singleton,
                                                   result]() -> nsresult {
        nsRefPtr<Pledge<nsCString>> p = singleton->mOutstandingPledges.Remove(id);
        if (!p) {
          return NS_ERROR_UNEXPECTED;
        }
        p->Resolve(result);
        return NS_OK;
      }), NS_DISPATCH_NORMAL);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }), NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsRefPtr<media::Parent> keepAlive(this);
  p->Then([this, keepAlive, aRequestId](const nsCString& aKey) mutable {
    if (!mDestroyed) {
      unused << SendGetOriginKeyResponse(aRequestId, aKey);
    }
    return NS_OK;
  });
  return true;
}

bool
Parent::RecvSanitizeOriginKeys(const uint64_t& aSinceWhen)
{
  nsRefPtr<ParentSingleton> singleton(mSingleton);

  // First, over to main to get profile dir.
  nsresult rv;

  rv = NS_DispatchToMainThread(NewRunnableFrom([singleton,
                                                aSinceWhen]() -> nsresult {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIFile> profileDir;
    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                         getter_AddRefs(profileDir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // Then from there over to stream-transport thread to do the file io.

    nsCOMPtr<nsIEventTarget> sts = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    MOZ_ASSERT(sts);
    rv = sts->Dispatch(NewRunnableFrom([profileDir, singleton, aSinceWhen]() -> nsresult {
      MOZ_ASSERT(!NS_IsMainThread());
      singleton->mOriginKeys.SetProfileDir(profileDir);
      singleton->mPrivateBrowsingOriginKeys.Clear(aSinceWhen);
      singleton->mOriginKeys.Clear(aSinceWhen);
      return NS_OK;
    }), NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  return true;
}

void
Parent::ActorDestroy(ActorDestroyReason aWhy)
{
  // No more IPC from here
  mDestroyed = true;
  LOG((__FUNCTION__));
}

Parent::Parent()
  : mSingleton(ParentSingleton::Get())
  , mDestroyed(false)
{
  if (!gMediaParentLog)
    gMediaParentLog = PR_NewLogModule("MediaParent");
  LOG(("media::Parent: %p", this));

  MOZ_COUNT_CTOR(Parent);
}

Parent::~Parent()
{
  LOG(("~media::Parent: %p", this));

  MOZ_COUNT_DTOR(Parent);
}

PMediaParent*
AllocPMediaParent()
{
  Parent* obj = new Parent();
  obj->AddRef();
  return obj;
}

bool
DeallocPMediaParent(media::PMediaParent *aActor)
{
  static_cast<Parent*>(aActor)->Release();
  return true;
}

}
}
