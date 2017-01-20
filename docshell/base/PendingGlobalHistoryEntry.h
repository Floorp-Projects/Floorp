/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PendingGlobalHistoryEntry_h_
#define mozilla_dom_PendingGlobalHistoryEntry_h_

#include "mozilla/IHistory.h"
#include "nsIGlobalHistory2.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {

namespace dom {

// This class acts as a wrapper around a IHistory, in that it can be used to
// record a series of operations on the IHistory, and then play them back at a
// later time. This is used in Prerendering in order to record the Global
// History changes being made by the prerendering docshell and then play them
// back when the docshell is finished rendering.
//
// This class also handles applying the changes to nsIGlobalHistory2.
class PendingGlobalHistoryEntry
{
public:
  void VisitURI(nsIURI* aURI,
                nsIURI* aLastVisitedURI,
                nsIURI* aReferrerURI,
                uint32_t aFlags);

  void SetURITitle(nsIURI* aURI,
                   const nsAString& aTitle);

  nsresult ApplyChanges(IHistory* aHistory);

  nsresult ApplyChanges(nsIGlobalHistory2* aHistory);

private:
  struct URIVisit
  {
    nsCOMPtr<nsIURI> mURI;
    nsCOMPtr<nsIURI> mLastVisitedURI;
    nsCOMPtr<nsIURI> mReferrerURI;
    uint32_t mFlags = 0;
  };
  struct URITitle
  {
    nsCOMPtr<nsIURI> mURI;
    nsString mTitle;
  };
  nsTArray<URIVisit> mVisits;
  nsTArray<URITitle> mTitles;
};

} // namespace dom

} // namespace mozilla

#endif // mozilla_dom_PendingGlobalHistoryEntry_h_

