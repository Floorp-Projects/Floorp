/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaManager.h"

#include "nsIFile.h"

#include "mozilla/ClearOnShutdown.h"
#include "nsComponentManagerUtils.h"
#include "xpcpublic.h"

#include "CheckQuotaHelper.h"

USING_QUOTA_NAMESPACE

namespace {

nsAutoPtr<QuotaManager> gInstance;

PLDHashOperator
RemoveQuotaForPatternCallback(const nsACString& aKey,
                              nsRefPtr<OriginInfo>& aValue,
                              void* aUserArg)
{
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");
  NS_ASSERTION(aUserArg, "Null pointer!");

  const nsACString* pattern =
    static_cast<const nsACString*>(aUserArg);

  if (StringBeginsWith(aKey, *pattern)) {
    return PL_DHASH_REMOVE;
  }

  return PL_DHASH_NEXT;
}

} // anonymous namespace

void
QuotaObject::AddRef()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    NS_ERROR("Null quota manager, this shouldn't happen, possible leak!");

    NS_AtomicIncrementRefcnt(mRefCnt);

    return;
  }

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  ++mRefCnt;
}

void
QuotaObject::Release()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    NS_ERROR("Null quota manager, this shouldn't happen, possible leak!");

    nsrefcnt count = NS_AtomicDecrementRefcnt(mRefCnt);
    if (count == 0) {
      mRefCnt = 1;
      delete this;
    }

    return;
  }

  {
    MutexAutoLock lock(quotaManager->mQuotaMutex);

    --mRefCnt;

    if (mRefCnt > 0) {
      return;
    }

    if (mOriginInfo) {
      mOriginInfo->mQuotaObjects.Remove(mPath);
    }
  }

  delete this;
}

void
QuotaObject::UpdateSize(int64_t aSize)
{
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  if (mOriginInfo) {
    mOriginInfo->mUsage -= mSize;
    mSize = aSize;
    mOriginInfo->mUsage += mSize;
  }
}

bool
QuotaObject::MaybeAllocateMoreSpace(int64_t aOffset, int32_t aCount)
{
  int64_t end = aOffset + aCount;

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  if (mSize >= end || !mOriginInfo) {
    return true;
  }

  int64_t newUsage = mOriginInfo->mUsage - mSize + end;
  if (newUsage > mOriginInfo->mLimit) {
    // This will block the thread, but it will also drop the mutex while
    // waiting. The mutex will be reacquired again when the waiting is finished.
    if (!quotaManager->LockedQuotaIsLifted()) {
      return false;
    }

    // Threads raced, the origin info removal has been done by some other
    // thread.
    if (!mOriginInfo) {
      // The other thread could allocate more space.
      if (end > mSize) {
        mSize = end;
      }

      return true;
    }

    nsCString origin = mOriginInfo->mOrigin;

    mOriginInfo->LockedClearOriginInfos();
    NS_ASSERTION(!mOriginInfo,
                 "Should have cleared in LockedClearOriginInfos!");

    quotaManager->mOriginInfos.Remove(origin);

    // Some other thread could increase the size without blocking (increasing
    // the origin usage without hitting the limit), but no more than this one.
    NS_ASSERTION(mSize < end, "This shouldn't happen!");

    mSize = end;

    return true;
  }

  mOriginInfo->mUsage = newUsage;
  mSize = end;

  return true;
}

#ifdef DEBUG
void
OriginInfo::LockedClearOriginInfos()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  quotaManager->mQuotaMutex.AssertCurrentThreadOwns();

  mQuotaObjects.EnumerateRead(ClearOriginInfoCallback, nullptr);
}
#endif

// static
PLDHashOperator
OriginInfo::ClearOriginInfoCallback(const nsAString& aKey,
                                    QuotaObject* aValue,
                                    void* aUserArg)
{
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");

  aValue->mOriginInfo = nullptr;

  return PL_DHASH_NEXT;
}

QuotaManager::QuotaManager()
: mCurrentWindowIndex(BAD_TLS_INDEX),
  mQuotaMutex("QuotaManager.mQuotaMutex")
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

QuotaManager::~QuotaManager()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

// static
QuotaManager*
QuotaManager::GetOrCreate()
{
  if (!gInstance) {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    nsAutoPtr<QuotaManager> instance(new QuotaManager());

    NS_ENSURE_TRUE(instance->Init(), nullptr);

    gInstance = instance.forget();

    ClearOnShutdown(&gInstance);
  }

  return gInstance;
}

// static
QuotaManager*
QuotaManager::Get()
{
  // Does not return an owning reference.
  return gInstance;
}

bool
QuotaManager::Init()
{
  // We need a thread-local to hold the current window.
  NS_ASSERTION(mCurrentWindowIndex == BAD_TLS_INDEX, "Huh?");

  if (PR_NewThreadPrivateIndex(&mCurrentWindowIndex, nullptr) != PR_SUCCESS) {
    NS_ERROR("PR_NewThreadPrivateIndex failed, QuotaManager disabled");
    mCurrentWindowIndex = BAD_TLS_INDEX;
    return false;
  }

  mOriginInfos.Init();
  mCheckQuotaHelpers.Init();

  return true;
}

void
QuotaManager::InitQuotaForOrigin(const nsACString& aOrigin,
                                 int64_t aLimit,
                                 int64_t aUsage)
{
  OriginInfo* info = new OriginInfo(aOrigin, aLimit * 1024 * 1024, aUsage);

  MutexAutoLock lock(mQuotaMutex);

  NS_ASSERTION(!mOriginInfos.GetWeak(aOrigin), "Replacing an existing entry!");
  mOriginInfos.Put(aOrigin, info);
}

void
QuotaManager::DecreaseUsageForOrigin(const nsACString& aOrigin,
                                     int64_t aSize)
{
  MutexAutoLock lock(mQuotaMutex);

  nsRefPtr<OriginInfo> originInfo;
  mOriginInfos.Get(aOrigin, getter_AddRefs(originInfo));

  if (originInfo) {
    originInfo->mUsage -= aSize;
  }
}

void
QuotaManager::RemoveQuotaForPattern(const nsACString& aPattern)
{
  NS_ASSERTION(!aPattern.IsEmpty(), "Empty pattern!");

  MutexAutoLock lock(mQuotaMutex);

  mOriginInfos.Enumerate(RemoveQuotaForPatternCallback,
                         const_cast<nsACString*>(&aPattern));
}

already_AddRefed<QuotaObject>
QuotaManager::GetQuotaObject(const nsACString& aOrigin,
                             nsIFile* aFile)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsString path;
  nsresult rv = aFile->GetPath(path);
  NS_ENSURE_SUCCESS(rv, nullptr);

  int64_t fileSize;

  bool exists;
  rv = aFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, nullptr);

  if (exists) {
    rv = aFile->GetFileSize(&fileSize);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }
  else {
    fileSize = 0;
  }

  QuotaObject* info = nullptr;
  {
    MutexAutoLock lock(mQuotaMutex);

    nsRefPtr<OriginInfo> originInfo;
    mOriginInfos.Get(aOrigin, getter_AddRefs(originInfo));

    if (!originInfo) {
      return nullptr;
    }

    originInfo->mQuotaObjects.Get(path, &info);

    if (!info) {
      info = new QuotaObject(originInfo, path, fileSize);
      originInfo->mQuotaObjects.Put(path, info);
    }
  }

  nsRefPtr<QuotaObject> result = info;
  return result.forget();
}

already_AddRefed<QuotaObject>
QuotaManager::GetQuotaObject(const nsACString& aOrigin,
                             const nsAString& aPath)
{
  nsresult rv;
  nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = file->InitWithPath(aPath);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return GetQuotaObject(aOrigin, file);
}

void
QuotaManager::SetCurrentWindowInternal(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(mCurrentWindowIndex != BAD_TLS_INDEX,
               "Should have a valid TLS storage index!");

  if (aWindow) {
    NS_ASSERTION(!PR_GetThreadPrivate(mCurrentWindowIndex),
                 "Somebody forgot to clear the current window!");
    PR_SetThreadPrivate(mCurrentWindowIndex, aWindow);
  }
  else {
    // We cannot assert PR_GetThreadPrivate(mCurrentWindowIndex) here because
    // there are some cases where we did not already have a window.
    PR_SetThreadPrivate(mCurrentWindowIndex, nullptr);
  }
}

void
QuotaManager::CancelPromptsForWindowInternal(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<CheckQuotaHelper> helper;

  MutexAutoLock autoLock(mQuotaMutex);

  if (mCheckQuotaHelpers.Get(aWindow, getter_AddRefs(helper))) {
    helper->Cancel();
  }
}

bool
QuotaManager::LockedQuotaIsLifted()
{
  mQuotaMutex.AssertCurrentThreadOwns();

  NS_ASSERTION(mCurrentWindowIndex != BAD_TLS_INDEX,
               "Should have a valid TLS storage index!");

  nsPIDOMWindow* window =
    static_cast<nsPIDOMWindow*>(PR_GetThreadPrivate(mCurrentWindowIndex));

  // Quota is not enforced in chrome contexts (e.g. for components and JSMs)
  // so we must have a window here.
  NS_ASSERTION(window, "Why don't we have a Window here?");

  bool createdHelper = false;

  nsRefPtr<CheckQuotaHelper> helper;
  if (!mCheckQuotaHelpers.Get(window, getter_AddRefs(helper))) {
    helper = new CheckQuotaHelper(window, mQuotaMutex);
    createdHelper = true;

    mCheckQuotaHelpers.Put(window, helper);

    // Unlock while calling out to XPCOM
    {
      MutexAutoUnlock autoUnlock(mQuotaMutex);

      nsresult rv = NS_DispatchToMainThread(helper);
      NS_ENSURE_SUCCESS(rv, false);
    }

    // Relocked.  If any other threads hit the quota limit on the same Window,
    // they are using the helper we created here and are now blocking in
    // PromptAndReturnQuotaDisabled.
  }

  bool result = helper->PromptAndReturnQuotaIsDisabled();

  // If this thread created the helper and added it to the hash, this thread
  // must remove it.
  if (createdHelper) {
    mCheckQuotaHelpers.Remove(window);
  }

  return result;
}
