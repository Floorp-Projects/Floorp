/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVListeners_h
#define mozilla_dom_TVListeners_h

#include "nsClassHashtable.h"
#include "nsITVService.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {
namespace dom {

class TVSource;

class TVSourceListener : public nsITVSourceListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITVSOURCELISTENER

  void RegisterSource(TVSource* aSource);

  void UnregisterSource(TVSource* aSource);

private:
  ~TVSourceListener() {}

  already_AddRefed<TVSource> GetSource(const nsAString& aTunerId,
                                       const nsAString& aSourceType);

  // The tuner ID acts as the key of the outer table; whereas the source type is
  // the key for the inner one.
  nsClassHashtable<nsStringHashKey, nsRefPtrHashtable<nsStringHashKey, TVSource>> mSources;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVListeners_h
