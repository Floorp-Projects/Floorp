/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationService_h
#define mozilla_dom_PresentationService_h

#include "nsClassHashtable.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsTObserverArray.h"
#include "PresentationSessionInfo.h"

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

private:
  ~PresentationService();
  void NotifyAvailableChange(bool aIsAvailable);

  nsClassHashtable<nsStringHashKey, PresentationSessionInfo> mSessionInfo;
  nsTObserverArray<nsCOMPtr<nsIPresentationListener>> mListeners;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationService_h
