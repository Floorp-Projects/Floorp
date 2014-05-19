/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IccListener_h
#define mozilla_dom_IccListener_h

#include "nsAutoPtr.h"
#include "nsIIccProvider.h"

namespace mozilla {
namespace dom {

class IccManager;
class Icc;

class IccListener : public nsIIccListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIICCLISTENER

  IccListener(IccManager* aIccManager, uint32_t aClientId);
  ~IccListener();

  void
  Shutdown();

  Icc*
  GetIcc()
  {
    return mIcc;
  }

private:
  uint32_t mClientId;
  // We did not setup 'mIcc' and 'mIccManager' being a participant of cycle
  // collection is because in Navigator->Invalidate() it will call
  // mIccManager->Shutdown(), then IccManager will call Shutdown() of each
  // IccListener, this will release the reference and break the cycle.
  nsRefPtr<Icc> mIcc;
  nsRefPtr<IccManager> mIccManager;
  // mProvider is a xpcom service and will be released at shutdown, so it
  // doesn't need to be cycle collected.
  nsCOMPtr<nsIIccProvider> mProvider;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_IccListener_h
