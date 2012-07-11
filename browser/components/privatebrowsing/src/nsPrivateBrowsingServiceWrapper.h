/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIPrivateBrowsingService.h"
#include "nsIObserver.h"
#include "mozilla/Attributes.h"

class nsIJSContextStack;

class nsPrivateBrowsingServiceWrapper MOZ_FINAL : public nsIPrivateBrowsingService,
                                                  public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRIVATEBROWSINGSERVICE
  NS_DECL_NSIOBSERVER

  nsresult Init();

private:
  nsCOMPtr<nsIPrivateBrowsingService> mPBService;
};
