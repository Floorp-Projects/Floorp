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
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIInputStream.h"
#include "nsILineInputStream.h"
#include "nsIOutputStream.h"
#include "nsISafeOutputStream.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsISupportsImpl.h"
#include "mozilla/Logging.h"

#undef LOG
mozilla::LazyLogModule gMediaParentLog("MediaParent");
#define LOG(args) MOZ_LOG(gMediaParentLog, mozilla::LogLevel::Debug, args)

// A file in the profile dir is used to persist mOriginKeys used to anonymize
// deviceIds to be unique per origin, to avoid them being supercookies.

#define ORIGINKEYS_FILE "enumerate_devices.txt"
#define ORIGINKEYS_VERSION "1"

namespace mozilla {
namespace media {

StaticMutex sOriginKeyStoreMutex;
static OriginKeyStore* sOriginKeyStore = nullptr;

class OriginKeyStore : public nsISupports {
  NS_DECL_THREADSAFE_ISUPPORTS
  class OriginKey {
   public:
    static const size_t DecodedLength = 18;
    static const size_t EncodedLength = DecodedLength * 4 / 3;

    explicit OriginKey(const nsACString& aKey,
                       int64_t aSecondsStamp = 0)  // 0 = temporal
        : mKey(aKey), mSecondsStamp(aSecondsStamp) {}

    nsCString mKey;  // Base64 encoded.
    int64_t mSecondsStamp;
  };

  class OriginKeysTable {
   public:
    OriginKeysTable() : mPersistCount(0) {}

    nsresult GetPrincipalKey(const ipc::PrincipalInfo& aPrincipalInfo,
                             nsCString& aResult, bool aPersist = false) {
      nsAutoCString principalString;
      PrincipalInfoToString(aPrincipalInfo, principalString);

      OriginKey* key;
      if (!mKeys.Get(principalString, &key)) {
        nsCString salt;  // Make a new one
        nsresult rv = GenerateRandomName(salt, OriginKey::EncodedLength);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        key = new OriginKey(salt);
        mKeys.Put(principalString, key);
      }
      if (aPersist && !key->mSecondsStamp) {
        key->mSecondsStamp = PR_Now() / PR_USEC_PER_SEC;
        mPersistCount++;
      }
      aResult = key->mKey;
      return NS_OK;
    }

    void Clear(int64_t aSinceWhen) {
      // Avoid int64_t* <-> void* casting offset
      OriginKey since(nsCString(), aSinceWhen / PR_USEC_PER_SEC);
      for (auto iter = mKeys.Iter(); !iter.Done(); iter.Next()) {
        auto originKey = iter.UserData();
        LOG((((originKey->mSecondsStamp >= since.mSecondsStamp)
                  ? "%s: REMOVE %" PRId64 " >= %" PRId64
                  : "%s: KEEP   %" PRId64 " < %" PRId64),
             __FUNCTION__, originKey->mSecondsStamp, since.mSecondsStamp));

        if (originKey->mSecondsStamp >= since.mSecondsStamp) {
          iter.Remove();
        }
      }
      mPersistCount = 0;
    }

   private:
    void PrincipalInfoToString(const ipc::PrincipalInfo& aPrincipalInfo,
                               nsACString& aString) {
      switch (aPrincipalInfo.type()) {
        case ipc::PrincipalInfo::TSystemPrincipalInfo:
          aString.AssignLiteral("[System Principal]");
          return;

        case ipc::PrincipalInfo::TNullPrincipalInfo: {
          const ipc::NullPrincipalInfo& info =
              aPrincipalInfo.get_NullPrincipalInfo();
          aString.Assign(info.spec());
          return;
        }

        case ipc::PrincipalInfo::TContentPrincipalInfo: {
          const ipc::ContentPrincipalInfo& info =
              aPrincipalInfo.get_ContentPrincipalInfo();
          aString.Assign(info.originNoSuffix());

          nsAutoCString suffix;
          info.attrs().CreateSuffix(suffix);
          aString.Append(suffix);
          return;
        }

        case ipc::PrincipalInfo::TExpandedPrincipalInfo: {
          const ipc::ExpandedPrincipalInfo& info =
              aPrincipalInfo.get_ExpandedPrincipalInfo();

          aString.AssignLiteral("[Expanded Principal [");

          for (uint32_t i = 0; i < info.allowlist().Length(); i++) {
            nsAutoCString str;
            PrincipalInfoToString(info.allowlist()[i], str);

            if (i != 0) {
              aString.AppendLiteral(", ");
            }

            aString.Append(str);
          }

          aString.AppendLiteral("]]");
          return;
        }

        default:
          MOZ_CRASH("Unknown PrincipalInfo type!");
      }
    }

   protected:
    nsClassHashtable<nsCStringHashKey, OriginKey> mKeys;
    size_t mPersistCount;
  };

  class OriginKeysLoader : public OriginKeysTable {
   public:
    OriginKeysLoader() = default;

    nsresult GetPrincipalKey(const ipc::PrincipalInfo& aPrincipalInfo,
                             nsCString& aResult, bool aPersist = false) {
      auto before = mPersistCount;
      nsresult rv =
          OriginKeysTable::GetPrincipalKey(aPrincipalInfo, aResult, aPersist);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (mPersistCount != before) {
        Save();
      }
      return NS_OK;
    }

    already_AddRefed<nsIFile> GetFile() {
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

    nsresult Read() {
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
      MOZ_ASSERT(!mPersistCount);

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
        const nsACString& s = Substring(line, f + 1);
        f = s.FindChar(' ');
        if (f < 0) {
          continue;
        }
        int64_t secondsstamp = nsCString(Substring(s, 0, f)).ToInteger64(&rv);
        if (NS_FAILED(rv)) {
          continue;
        }
        const nsACString& origin = Substring(s, f + 1);

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
      mPersistCount = mKeys.Count();
      return NS_OK;
    }

    nsresult Write() {
      nsCOMPtr<nsIFile> file = GetFile();
      if (NS_WARN_IF(!file)) {
        return NS_ERROR_UNEXPECTED;
      }

      nsCOMPtr<nsIOutputStream> stream;
      nsresult rv =
          NS_NewSafeLocalFileOutputStream(getter_AddRefs(stream), file);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsAutoCString versionBuffer;
      versionBuffer.AppendLiteral(ORIGINKEYS_VERSION);
      versionBuffer.Append('\n');

      uint32_t count;
      rv = stream->Write(versionBuffer.Data(), versionBuffer.Length(), &count);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (count != versionBuffer.Length()) {
        return NS_ERROR_UNEXPECTED;
      }
      for (auto iter = mKeys.Iter(); !iter.Done(); iter.Next()) {
        const nsACString& origin = iter.Key();
        OriginKey* originKey = iter.UserData();

        if (!originKey->mSecondsStamp) {
          continue;  // don't write temporal ones
        }

        nsCString originBuffer;
        originBuffer.Append(originKey->mKey);
        originBuffer.Append(' ');
        originBuffer.AppendInt(originKey->mSecondsStamp);
        originBuffer.Append(' ');
        originBuffer.Append(origin);
        originBuffer.Append('\n');

        rv = stream->Write(originBuffer.Data(), originBuffer.Length(), &count);
        if (NS_WARN_IF(NS_FAILED(rv)) || count != originBuffer.Length()) {
          break;
        }
      }

      nsCOMPtr<nsISafeOutputStream> safeStream = do_QueryInterface(stream);
      MOZ_ASSERT(safeStream);

      rv = safeStream->Finish();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }

    nsresult Load() {
      nsresult rv = Read();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        Delete();
      }
      return rv;
    }

    nsresult Save() {
      nsresult rv = Write();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        NS_WARNING("Failed to write data for EnumerateDevices id-persistence.");
        Delete();
      }
      return rv;
    }

    void Clear(int64_t aSinceWhen) {
      OriginKeysTable::Clear(aSinceWhen);
      Delete();
      Save();
    }

    nsresult Delete() {
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

    void SetProfileDir(nsIFile* aProfileDir) {
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
  virtual ~OriginKeyStore() {
    StaticMutexAutoLock lock(sOriginKeyStoreMutex);
    sOriginKeyStore = nullptr;
    LOG(("%s", __FUNCTION__));
  }

 public:
  static OriginKeyStore* Get() {
    MOZ_ASSERT(NS_IsMainThread());
    StaticMutexAutoLock lock(sOriginKeyStoreMutex);
    if (!sOriginKeyStore) {
      sOriginKeyStore = new OriginKeyStore();
    }
    return sOriginKeyStore;
  }

  // Only accessed on StreamTS thread
  OriginKeysLoader mOriginKeys;
  OriginKeysTable mPrivateBrowsingOriginKeys;
};

NS_IMPL_ISUPPORTS0(OriginKeyStore)

template <class Super>
mozilla::ipc::IPCResult Parent<Super>::RecvGetPrincipalKey(
    const ipc::PrincipalInfo& aPrincipalInfo, const bool& aPersist,
    PMediaParent::GetPrincipalKeyResolver&& aResolve) {
  MOZ_ASSERT(NS_IsMainThread());

  // First, get profile dir.

  nsCOMPtr<nsIFile> profileDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(profileDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPCResult(this, false);
  }

  // Resolver has to be called in MainThread but the key is discovered
  // in a different thread. We wrap the resolver around a MozPromise to make
  // it more flexible and pass it to the new task. When this is done the
  // resolver is resolved in MainThread.

  // Then over to stream-transport thread (a thread pool) to do the actual
  // file io. Stash a promise to hold the answer and get an id for this request.

  nsCOMPtr<nsIEventTarget> sts =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(sts);
  auto taskQueue = MakeRefPtr<TaskQueue>(sts.forget(), "RecvGetPrincipalKey");
  RefPtr<Parent<Super>> that(this);

  InvokeAsync(
      taskQueue, __func__,
      [that, profileDir, aPrincipalInfo, aPersist]() {
        MOZ_ASSERT(!NS_IsMainThread());

        StaticMutexAutoLock lock(sOriginKeyStoreMutex);
        if (!sOriginKeyStore) {
          return PrincipalKeyPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                      __func__);
        }
        sOriginKeyStore->mOriginKeys.SetProfileDir(profileDir);

        nsresult rv;
        nsAutoCString result;
        if (IsPrincipalInfoPrivate(aPrincipalInfo)) {
          rv = sOriginKeyStore->mPrivateBrowsingOriginKeys.GetPrincipalKey(
              aPrincipalInfo, result);
        } else {
          rv = sOriginKeyStore->mOriginKeys.GetPrincipalKey(aPrincipalInfo,
                                                            result, aPersist);
        }

        if (NS_WARN_IF(NS_FAILED(rv))) {
          return PrincipalKeyPromise::CreateAndReject(rv, __func__);
        }
        return PrincipalKeyPromise::CreateAndResolve(result, __func__);
      })
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [aResolve](const PrincipalKeyPromise::ResolveOrRejectValue& aValue) {
            if (aValue.IsReject()) {
              aResolve(NS_LITERAL_CSTRING(""));
            } else {
              aResolve(aValue.ResolveValue());
            }
          });

  return IPC_OK();
}

template <class Super>
mozilla::ipc::IPCResult Parent<Super>::RecvSanitizeOriginKeys(
    const uint64_t& aSinceWhen, const bool& aOnlyPrivateBrowsing) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIFile> profileDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(profileDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPCResult(this, false);
  }
  // Over to stream-transport thread (a thread pool) to do the file io.

  nsCOMPtr<nsIEventTarget> sts =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(sts);

  rv = sts->Dispatch(
      NewRunnableFrom(
          [profileDir, aSinceWhen, aOnlyPrivateBrowsing]() -> nsresult {
            MOZ_ASSERT(!NS_IsMainThread());
            StaticMutexAutoLock lock(sOriginKeyStoreMutex);
            if (!sOriginKeyStore) {
              return NS_ERROR_FAILURE;
            }
            sOriginKeyStore->mPrivateBrowsingOriginKeys.Clear(aSinceWhen);
            if (!aOnlyPrivateBrowsing) {
              sOriginKeyStore->mOriginKeys.SetProfileDir(profileDir);
              sOriginKeyStore->mOriginKeys.Clear(aSinceWhen);
            }
            return NS_OK;
          }),
      NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPCResult(this, false);
  }
  return IPC_OK();
}

template <class Super>
void Parent<Super>::ActorDestroy(ActorDestroyReason aWhy) {
  // No more IPC from here
  mDestroyed = true;
  LOG(("%s", __FUNCTION__));
}

template <class Super>
Parent<Super>::Parent()
    : mOriginKeyStore(OriginKeyStore::Get()), mDestroyed(false) {
  LOG(("media::Parent: %p", this));
}

template <class Super>
Parent<Super>::~Parent() {
  LOG(("~media::Parent: %p", this));
}

PMediaParent* AllocPMediaParent() {
  Parent<PMediaParent>* obj = new Parent<PMediaParent>();
  obj->AddRef();
  return obj;
}

bool DeallocPMediaParent(media::PMediaParent* aActor) {
  static_cast<Parent<PMediaParent>*>(aActor)->Release();
  return true;
}

}  // namespace media
}  // namespace mozilla

// Instantiate templates to satisfy linker
template class mozilla::media::Parent<mozilla::media::NonE10s>;
