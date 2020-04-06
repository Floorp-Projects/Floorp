/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasePrincipal.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIBrowserDOMWindow.h"
#include "nsFrameLoaderOwner.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIPrincipal.h"
#include "nsIReferrerInfo.h"
#include "nsString.h"

namespace mozilla {
class OriginAttributes;
namespace dom {
class Element;
}  // namespace dom
}  // namespace mozilla

class nsOpenURIInFrameParams final : public nsIOpenURIInFrameParams {
 public:
  NS_DECL_CYCLE_COLLECTION_CLASS(nsOpenURIInFrameParams)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIOPENURIINFRAMEPARAMS

  explicit nsOpenURIInFrameParams(
      const mozilla::OriginAttributes& aOriginAttributes,
      mozilla::dom::Element* aOpener);

 private:
  ~nsOpenURIInFrameParams();

  mozilla::OriginAttributes mOpenerOriginAttributes;
  RefPtr<mozilla::dom::Element> mOpenerBrowser;
  nsCOMPtr<nsIReferrerInfo> mReferrerInfo;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  nsCOMPtr<nsIContentSecurityPolicy> mCsp;
};
