/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationServiceBase_h
#define mozilla_dom_PresentationServiceBase_h

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
  virtual ~PresentationServiceBase() = default;

  void Shutdown()
  {
    mRespondingListeners.Clear();
    mRespondingSessionIds.Clear();
    mRespondingWindowIds.Clear();
  }
  nsresult GetExistentSessionIdAtLaunchInternal(uint64_t aWindowId, nsAString& aSessionId);
  nsresult GetWindowIdBySessionIdInternal(const nsAString& aSessionId, uint64_t* aWindowId);
  void AddRespondingSessionId(uint64_t aWindowId, const nsAString& aSessionId);
  void RemoveRespondingSessionId(const nsAString& aSessionId);

  // Store the responding listener based on the window ID of the (in-process or
  // OOP) receiver page.
  nsRefPtrHashtable<nsUint64HashKey, nsIPresentationRespondingListener> mRespondingListeners;

  // Store the mapping between the window ID of the in-process and OOP page and the ID
  // of the responding session. It's used for an receiver page to retrieve
  // the correspondent session ID. Besides, also keep the mapping
  // between the responding session ID and the window ID to help look up the
  // window ID.
  nsClassHashtable<nsUint64HashKey, nsTArray<nsString>> mRespondingSessionIds;
  nsDataHashtable<nsStringHashKey, uint64_t> mRespondingWindowIds;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationServiceBase_h
