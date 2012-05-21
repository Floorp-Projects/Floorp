/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ThirdPartyUtil_h__
#define ThirdPartyUtil_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "mozIThirdPartyUtil.h"
#include "nsIEffectiveTLDService.h"

class nsIURI;
class nsIChannel;
class nsIDOMWindow;

class ThirdPartyUtil : public mozIThirdPartyUtil
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZITHIRDPARTYUTIL

  nsresult Init();

private:
  nsresult IsThirdPartyInternal(const nsCString& aFirstDomain,
    nsIURI* aSecondURI, bool* aResult);
  static already_AddRefed<nsIURI> GetURIFromWindow(nsIDOMWindow* aWin);

  nsCOMPtr<nsIEffectiveTLDService> mTLDService;
};

#endif

