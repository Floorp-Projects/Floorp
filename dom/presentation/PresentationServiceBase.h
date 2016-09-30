/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationServiceBase_h
#define mozilla_dom_PresentationServiceBase_h

#include "nsClassHashtable.h"
#include "nsIPresentationService.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsTArray.h"

class nsIPresentationRespondingListener;

namespace mozilla {
namespace dom {

template<class T>
class PresentationServiceBase
{
public:
  PresentationServiceBase() = default;

  already_AddRefed<T>
  GetSessionInfo(const nsAString& aSessionId, const uint8_t aRole)
  {
    MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
               aRole == nsIPresentationService::ROLE_RECEIVER);

    RefPtr<T> info;
    if (aRole == nsIPresentationService::ROLE_CONTROLLER) {
      return mSessionInfoAtController.Get(aSessionId, getter_AddRefs(info)) ?
             info.forget() : nullptr;
    } else {
      return mSessionInfoAtReceiver.Get(aSessionId, getter_AddRefs(info)) ?
             info.forget() : nullptr;
    }
  }

protected:
  class SessionIdManager final
  {
  public:
    explicit SessionIdManager()
    {
      MOZ_COUNT_CTOR(SessionIdManager);
    }

    ~SessionIdManager()
    {
      MOZ_COUNT_DTOR(SessionIdManager);
    }

    nsresult GetWindowId(const nsAString& aSessionId, uint64_t* aWindowId)
    {
      MOZ_ASSERT(NS_IsMainThread());

      if (mRespondingWindowIds.Get(aSessionId, aWindowId)) {
        return NS_OK;
      }
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsresult GetSessionIds(uint64_t aWindowId, nsTArray<nsString>& aSessionIds)
    {
      MOZ_ASSERT(NS_IsMainThread());

      nsTArray<nsString>* sessionIdArray;
      if (!mRespondingSessionIds.Get(aWindowId, &sessionIdArray)) {
        return NS_ERROR_INVALID_ARG;
      }

      aSessionIds.Assign(*sessionIdArray);
      return NS_OK;
    }

    void AddSessionId(uint64_t aWindowId, const nsAString& aSessionId)
    {
      MOZ_ASSERT(NS_IsMainThread());

      if (NS_WARN_IF(aWindowId == 0)) {
        return;
      }

      nsTArray<nsString>* sessionIdArray;
      if (!mRespondingSessionIds.Get(aWindowId, &sessionIdArray)) {
        sessionIdArray = new nsTArray<nsString>();
        mRespondingSessionIds.Put(aWindowId, sessionIdArray);
      }

      sessionIdArray->AppendElement(nsString(aSessionId));
      mRespondingWindowIds.Put(aSessionId, aWindowId);
    }

    void RemoveSessionId(const nsAString& aSessionId)
    {
      MOZ_ASSERT(NS_IsMainThread());

      uint64_t windowId = 0;
      if (mRespondingWindowIds.Get(aSessionId, &windowId)) {
        mRespondingWindowIds.Remove(aSessionId);
        nsTArray<nsString>* sessionIdArray;
        if (mRespondingSessionIds.Get(windowId, &sessionIdArray)) {
          sessionIdArray->RemoveElement(nsString(aSessionId));
          if (sessionIdArray->IsEmpty()) {
            mRespondingSessionIds.Remove(windowId);
          }
        }
      }
    }

    nsresult UpdateWindowId(const nsAString& aSessionId, const uint64_t aWindowId)
    {
      MOZ_ASSERT(NS_IsMainThread());

      RemoveSessionId(aSessionId);
      AddSessionId(aWindowId, aSessionId);
      return NS_OK;
    }

    void Clear()
    {
      mRespondingSessionIds.Clear();
      mRespondingWindowIds.Clear();
    }

  private:
    nsClassHashtable<nsUint64HashKey, nsTArray<nsString>> mRespondingSessionIds;
    nsDataHashtable<nsStringHashKey, uint64_t> mRespondingWindowIds;
  };

  virtual ~PresentationServiceBase() = default;

  void Shutdown()
  {
    mRespondingListeners.Clear();
    mControllerSessionIdManager.Clear();
    mReceiverSessionIdManager.Clear();
  }

  nsresult GetWindowIdBySessionIdInternal(const nsAString& aSessionId,
                                          uint8_t aRole,
                                          uint64_t* aWindowId)
  {
    MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
               aRole == nsIPresentationService::ROLE_RECEIVER);

    if (NS_WARN_IF(!aWindowId)) {
      return NS_ERROR_INVALID_POINTER;
    }

    if (aRole == nsIPresentationService::ROLE_CONTROLLER) {
      return mControllerSessionIdManager.GetWindowId(aSessionId, aWindowId);
    }

    return mReceiverSessionIdManager.GetWindowId(aSessionId, aWindowId);
  }

  void AddRespondingSessionId(uint64_t aWindowId,
                              const nsAString& aSessionId,
                              uint8_t aRole)
  {
    MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
               aRole == nsIPresentationService::ROLE_RECEIVER);

    if (aRole == nsIPresentationService::ROLE_CONTROLLER) {
      mControllerSessionIdManager.AddSessionId(aWindowId, aSessionId);
    } else {
      mReceiverSessionIdManager.AddSessionId(aWindowId, aSessionId);
    }
  }

  void RemoveRespondingSessionId(const nsAString& aSessionId,
                                 uint8_t aRole)
  {
    MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
               aRole == nsIPresentationService::ROLE_RECEIVER);

    if (aRole == nsIPresentationService::ROLE_CONTROLLER) {
      mControllerSessionIdManager.RemoveSessionId(aSessionId);
    } else {
      mReceiverSessionIdManager.RemoveSessionId(aSessionId);
    }
  }

  nsresult UpdateWindowIdBySessionIdInternal(const nsAString& aSessionId,
                                             uint8_t aRole,
                                             const uint64_t aWindowId)
  {
    MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
               aRole == nsIPresentationService::ROLE_RECEIVER);

    if (aRole == nsIPresentationService::ROLE_CONTROLLER) {
      return mControllerSessionIdManager.UpdateWindowId(aSessionId, aWindowId);
    }

    return mReceiverSessionIdManager.UpdateWindowId(aSessionId, aWindowId);
  }

  // Store the responding listener based on the window ID of the (in-process or
  // OOP) receiver page.
  nsRefPtrHashtable<nsUint64HashKey, nsIPresentationRespondingListener>
  mRespondingListeners;

  // Store the mapping between the window ID of the in-process and OOP page and the ID
  // of the responding session. It's used for both controller and receiver page
  // to retrieve the correspondent session ID. Besides, also keep the mapping
  // between the responding session ID and the window ID to help look up the
  // window ID.
  SessionIdManager mControllerSessionIdManager;
  SessionIdManager mReceiverSessionIdManager;

  nsRefPtrHashtable<nsStringHashKey, T> mSessionInfoAtController;
  nsRefPtrHashtable<nsStringHashKey, T> mSessionInfoAtReceiver;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationServiceBase_h
