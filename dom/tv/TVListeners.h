/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVListeners_h
#define mozilla_dom_TVListeners_h

#include "nsCycleCollectionParticipant.h"
#include "nsITVService.h"

namespace mozilla {
namespace dom {

class TVSource;

/*
 * Instead of making |TVSource| class implement |TVSource| (WebIDL) and
 * |nsITVSourceListener| (XPCOM) at the same time, having an individual class
 * for |nsITVSourceListener| (XPCOM) interface would help the JS context
 * recognize |nsITVSourceListener| instances (when it comes to use them in
 * |TVSimulatorService.js|.)
 */
class TVSourceListener final : public nsITVSourceListener
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(TVSourceListener)
  NS_DECL_NSITVSOURCELISTENER

  static already_AddRefed<TVSourceListener> Create(TVSource* aSource);

private:
  explicit TVSourceListener(TVSource* aSource);

  ~TVSourceListener();

  bool Init();

  void Shutdown();

  bool IsMatched(const nsAString& aTunerId, const nsAString& aSourceType);

  RefPtr<TVSource> mSource;

  // Store the tuner ID for |UnregisterSourceListener| call in |Shutdown| since
  // |mSource->Tuner()| may not exist at that moment.
  nsString mTunerId;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVListeners_h
