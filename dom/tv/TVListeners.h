/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVListeners_h
#define mozilla_dom_TVListeners_h

#include "mozilla/dom/TVSource.h"
#include "nsCycleCollectionParticipant.h"
#include "nsITVService.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class TVSourceListener final : public nsITVSourceListener
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(TVSourceListener)
  NS_DECL_NSITVSOURCELISTENER

  void RegisterSource(TVSource* aSource);

  void UnregisterSource(TVSource* aSource);

private:
  ~TVSourceListener() {}

  already_AddRefed<TVSource> GetSource(const nsAString& aTunerId,
                                       const nsAString& aSourceType);

  nsTArray<RefPtr<TVSource>> mSources;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVListeners_h
