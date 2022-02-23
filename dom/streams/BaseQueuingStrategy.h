/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BaseQueuingStrategy_h
#define mozilla_dom_BaseQueuingStrategy_h

#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"

namespace mozilla::dom {

class BaseQueuingStrategy : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(BaseQueuingStrategy)

  BaseQueuingStrategy(nsISupports* aGlobal, double aHighWaterMark)
      : mGlobal(do_QueryInterface(aGlobal)), mHighWaterMark(aHighWaterMark) {}

  nsIGlobalObject* GetParentObject() const;

  double HighWaterMark() const { return mHighWaterMark; }

 protected:
  virtual ~BaseQueuingStrategy() = default;

 protected:
  nsCOMPtr<nsIGlobalObject> mGlobal;
  double mHighWaterMark = 0.0;
};

}  // namespace mozilla::dom

#endif
