/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IccListener_h
#define mozilla_dom_IccListener_h

#include "nsIIccService.h"

namespace mozilla {
namespace dom {

class IccManager;
class Icc;

class IccListener final : public nsIIccListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIICCLISTENER

  IccListener(IccManager* aIccManager, uint32_t aClientId);

  void
  Shutdown();

  Icc*
  GetIcc()
  {
    return mIcc;
  }

private:
  ~IccListener();

private:
  uint32_t mClientId;
  // We did not setup 'mIcc' and 'mIccManager' being a participant of cycle
  // collection is because in Navigator->Invalidate() it will call
  // mIccManager->Shutdown(), then IccManager will call Shutdown() of each
  // IccListener, this will release the reference and break the cycle.
  RefPtr<Icc> mIcc;
  RefPtr<IccManager> mIccManager;
  // mHandler will be released at Shutdown(), there is no need to join cycle
  // collection.
  nsCOMPtr<nsIIcc> mHandler;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_IccListener_h
