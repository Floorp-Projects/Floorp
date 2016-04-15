/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVGonkService_h
#define mozilla_dom_TVGonkService_h

#include "nsCOMPtr.h"
#include "nsITVService.h"
#include "nsTArray.h"
#include "TVTypes.h"

namespace mozilla {
namespace dom {

class TVGonkService final : public nsITVService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITVSERVICE

  TVGonkService();

private:
  ~TVGonkService();

  void GetSourceListeners(
    const nsAString& aTunerId, const nsAString& aSourceType,
    nsTArray<nsCOMPtr<nsITVSourceListener> >& aListeners) const;

  nsTArray<UniquePtr<TVSourceListenerTuple>> mSourceListenerTuples;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVGonkService_h
