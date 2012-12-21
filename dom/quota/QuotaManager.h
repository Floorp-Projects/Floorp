/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_quotamanager_h__
#define mozilla_dom_quota_quotamanager_h__

#include "QuotaCommon.h"

#include "mozilla/Mutex.h"
#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsThreadUtils.h"

class nsPIDOMWindow;

BEGIN_QUOTA_NAMESPACE

class CheckQuotaHelper;
class OriginInfo;
class QuotaManager;

class QuotaObject
{
  friend class OriginInfo;
  friend class QuotaManager;

public:
  void
  AddRef();

  void
  Release();

  void
  UpdateSize(int64_t aSize);

  bool
  MaybeAllocateMoreSpace(int64_t aOffset, int32_t aCount);

private:
  QuotaObject(OriginInfo* aOriginInfo, const nsAString& aPath, int64_t aSize)
  : mOriginInfo(aOriginInfo), mPath(aPath), mSize(aSize)
  { }

  virtual ~QuotaObject()
  { }

  nsAutoRefCnt mRefCnt;

  OriginInfo* mOriginInfo;
  nsString mPath;
  int64_t mSize;
};

class OriginInfo
{
  friend class QuotaManager;
  friend class QuotaObject;

public:
  OriginInfo(const nsACString& aOrigin, int64_t aLimit, int64_t aUsage)
  : mOrigin(aOrigin), mLimit(aLimit), mUsage(aUsage)
  {
    mQuotaObjects.Init();
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OriginInfo)

private:
  void
#ifdef DEBUG
  LockedClearOriginInfos();
#else
  LockedClearOriginInfos()
  {
    mQuotaObjects.EnumerateRead(ClearOriginInfoCallback, nullptr);
  }
#endif

  static PLDHashOperator
  ClearOriginInfoCallback(const nsAString& aKey,
                          QuotaObject* aValue, void* aUserArg);

  nsDataHashtable<nsStringHashKey, QuotaObject*> mQuotaObjects;

  nsCString mOrigin;
  int64_t mLimit;
  int64_t mUsage;
};

class QuotaManager
{
  friend class nsAutoPtr<QuotaManager>;
  friend class OriginInfo;
  friend class QuotaObject;

public:
  // Returns a non-owning reference.
  static QuotaManager*
  GetOrCreate();

  // Returns a non-owning reference.
  static QuotaManager*
  Get();

  void
  InitQuotaForOrigin(const nsACString& aOrigin,
                     int64_t aLimit,
                     int64_t aUsage);

  void
  DecreaseUsageForOrigin(const nsACString& aOrigin,
                         int64_t aSize);

  void
  RemoveQuotaForPattern(const nsACString& aPattern);

  already_AddRefed<QuotaObject>
  GetQuotaObject(const nsACString& aOrigin,
                 nsIFile* aFile);

  already_AddRefed<QuotaObject>
  GetQuotaObject(const nsACString& aOrigin,
                 const nsAString& aPath);

  // Set the Window that the current thread is doing operations for.
  // The caller is responsible for ensuring that aWindow is held alive.
  static void
  SetCurrentWindow(nsPIDOMWindow* aWindow)
  {
    QuotaManager* quotaManager = Get();
    NS_ASSERTION(quotaManager, "Must have a manager here!");

    quotaManager->SetCurrentWindowInternal(aWindow);
  }

  static void
  CancelPromptsForWindow(nsPIDOMWindow* aWindow)
  {
    NS_ASSERTION(aWindow, "Passed null window!");

    QuotaManager* quotaManager = Get();
    NS_ASSERTION(quotaManager, "Must have a manager here!");

    quotaManager->CancelPromptsForWindowInternal(aWindow);
  }

private:
  QuotaManager();

  virtual ~QuotaManager();

  bool
  Init();

  void
  SetCurrentWindowInternal(nsPIDOMWindow* aWindow);

  void
  CancelPromptsForWindowInternal(nsPIDOMWindow* aWindow);

  // Determine if the quota is lifted for the Window the current thread is
  // using.
  bool
  LockedQuotaIsLifted();

  // TLS storage index for the current thread's window.
  unsigned int mCurrentWindowIndex;

  mozilla::Mutex mQuotaMutex;

  nsRefPtrHashtable<nsCStringHashKey, OriginInfo> mOriginInfos;

  // A map of Windows to the corresponding quota helper.
  nsRefPtrHashtable<nsPtrHashKey<nsPIDOMWindow>,
                    CheckQuotaHelper> mCheckQuotaHelpers;
};

class AutoEnterWindow
{
public:
  AutoEnterWindow(nsPIDOMWindow* aWindow)
  {
    QuotaManager::SetCurrentWindow(aWindow);
  }

  ~AutoEnterWindow()
  {
    QuotaManager::SetCurrentWindow(nullptr);
  }
};

END_QUOTA_NAMESPACE

#endif /* mozilla_dom_quota_quotamanager_h__ */
