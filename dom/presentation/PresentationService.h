/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationService_h
#define mozilla_dom_PresentationService_h

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsRefPtrHashtable.h"
#include "nsTObserverArray.h"
#include "PresentationSessionInfo.h"

class nsIPresentationSessionRequest;
class nsIURI;

namespace mozilla {
namespace dom {

class PresentationService final : public nsIPresentationService
                                , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIPRESENTATIONSERVICE

  PresentationService();
  bool Init();

  already_AddRefed<PresentationSessionInfo>
  GetSessionInfo(const nsAString& aSessionId)
  {
    nsRefPtr<PresentationSessionInfo> info;
    return mSessionInfo.Get(aSessionId, getter_AddRefs(info)) ?
           info.forget() : nullptr;
  }

  bool IsSessionAccessible(const nsAString& aSessionId,
                           base::ProcessId aProcessId);

private:
  ~PresentationService();
  void HandleShutdown();
  nsresult HandleDeviceChange();
  nsresult HandleSessionRequest(nsIPresentationSessionRequest* aRequest);
  void NotifyAvailableChange(bool aIsAvailable);
  bool IsAppInstalled(nsIURI* aUri);

  bool mIsAvailable;
  nsRefPtrHashtable<nsStringHashKey, PresentationSessionInfo> mSessionInfo;
  nsTObserverArray<nsCOMPtr<nsIPresentationListener>> mListeners;

  // Store the mapping between the window ID of the in-process page and the ID
  // of the responding session. It's used for an in-process receiver page to
  // retrieve the correspondent session ID. Besides, also keep the mapping
  // between the responding session ID and the window ID to help look up the
  // window ID.
  nsClassHashtable<nsUint64HashKey, nsString> mRespondingSessionIds;
  nsDataHashtable<nsStringHashKey, uint64_t> mRespondingWindowIds;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationService_h
