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
#include "prlog.h"

#undef LOG
#if defined(PR_LOGGING)
PRLogModuleInfo *gMediaParentLog;
#define LOG(args) PR_LOG(gMediaParentLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif

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
    OriginKeysLoader()
    {
      Load();
    }

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
      if (!mProfileDir) {
        nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                             getter_AddRefs(mProfileDir));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return nullptr;
        }
      }
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

  OriginKeysLoader mOriginKeys;
  OriginKeysTable mPrivateBrowsingOriginKeys;
};

NS_IMPL_ISUPPORTS0(ParentSingleton)

bool
Parent::RecvGetOriginKey(const uint32_t& aRequestId,
                         const nsCString& aOrigin,
                         const bool& aPrivateBrowsing)
{
  // Hand over to stream-transport thread.

  nsCOMPtr<nsIEventTarget> sts = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(sts);
  nsRefPtr<ParentSingleton> singleton(mSingleton);

  nsRefPtr<PledgeRunnable<nsCString>> p = PledgeRunnable<nsCString>::New(
      [singleton, aOrigin, aPrivateBrowsing](nsCString& aResult) {
    if (aPrivateBrowsing) {
      singleton->mPrivateBrowsingOriginKeys.GetOriginKey(aOrigin, aResult);
    } else {
      singleton->mOriginKeys.GetOriginKey(aOrigin, aResult);
    }
    return NS_OK;
  });
  nsresult rv = sts->Dispatch(p, NS_DISPATCH_NORMAL);
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
  // Hand over to stream-transport thread.

  nsCOMPtr<nsIEventTarget> sts = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(sts);
  nsRefPtr<ParentSingleton> singleton(mSingleton);

  nsRefPtr<PledgeRunnable<bool>> p = PledgeRunnable<bool>::New(
      [singleton, aSinceWhen](bool) {
    singleton->mPrivateBrowsingOriginKeys.Clear(aSinceWhen);
    singleton->mOriginKeys.Clear(aSinceWhen);
    return NS_OK;
  });
  nsresult rv = sts->Dispatch(p, NS_DISPATCH_NORMAL);
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
#if defined(PR_LOGGING)
  if (!gMediaParentLog)
    gMediaParentLog = PR_NewLogModule("MediaParent");
#endif
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
