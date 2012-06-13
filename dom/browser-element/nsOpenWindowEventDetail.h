/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIOpenWindowEventDetail.h"
#include "nsIDOMNode.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsString.h"

/**
 * When we send a mozbrowseropenwindow event (an instance of CustomEvent), we
 * use an instance of this class as the event's detail.
 */
class nsOpenWindowEventDetail : public nsIOpenWindowEventDetail
{
public:
  nsOpenWindowEventDetail(const nsAString& aURL,
                          const nsAString& aName,
                          const nsAString& aFeatures,
                          nsIDOMNode* aFrameElement)
    : mURL(aURL)
    , mName(aName)
    , mFeatures(aFeatures)
    , mFrameElement(aFrameElement)
  {}

  virtual ~nsOpenWindowEventDetail() {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsOpenWindowEventDetail)
  NS_DECL_NSIOPENWINDOWEVENTDETAIL

private:
  const nsString mURL;
  const nsString mName;
  const nsString mFeatures;
  nsCOMPtr<nsIDOMNode> mFrameElement;
};
