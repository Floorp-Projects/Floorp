/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationServiceBase_h
#define mozilla_dom_PresentationServiceBase_h

#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsTArray.h"

class nsIPresentationRespondingListener;
class nsString;

namespace mozilla {
namespace dom {

class PresentationServiceBase : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  PresentationServiceBase() = default;

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

    nsresult GetWindowId(const nsAString& aSessionId, uint64_t* aWindowId);

    nsresult GetSessionIds(uint64_t aWindowId, nsTArray<nsString>& aSessionIds);

    void AddSessionId(uint64_t aWindowId, const nsAString& aSessionId);

    void RemoveSessionId(const nsAString& aSessionId);

    nsresult UpdateWindowId(const nsAString& aSessionId, const uint64_t aWindowId);

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
                                          uint64_t* aWindowId);
  void AddRespondingSessionId(uint64_t aWindowId,
                              const nsAString& aSessionId,
                              uint8_t aRole);
  void RemoveRespondingSessionId(const nsAString& aSessionId,
                                 uint8_t aRole);
  nsresult UpdateWindowIdBySessionIdInternal(const nsAString& aSessionId,
                                             uint8_t aRole,
                                             const uint64_t aWindowId);

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
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationServiceBase_h
