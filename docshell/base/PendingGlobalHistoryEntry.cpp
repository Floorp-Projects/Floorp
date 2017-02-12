#include "mozilla/dom/PendingGlobalHistoryEntry.h"

namespace mozilla {

namespace dom {

void
PendingGlobalHistoryEntry::VisitURI(nsIURI* aURI,
                             nsIURI* aLastVisitedURI,
                             nsIURI* aReferrerURI,
                             uint32_t aFlags)
{
  URIVisit visit;
  visit.mURI = aURI;
  visit.mLastVisitedURI = aLastVisitedURI;
  visit.mReferrerURI = aReferrerURI;
  visit.mFlags = aFlags;
  mVisits.AppendElement(Move(visit));
}

void
PendingGlobalHistoryEntry::SetURITitle(nsIURI* aURI,
                                const nsAString& aTitle)
{
  URITitle title;
  title.mURI = aURI;
  title.mTitle.Assign(aTitle);
  mTitles.AppendElement(title);
}

nsresult
PendingGlobalHistoryEntry::ApplyChanges(IHistory* aHistory)
{
  nsresult rv;
  for (const URIVisit& visit : mVisits) {
    rv = aHistory->VisitURI(visit.mURI, visit.mLastVisitedURI, visit.mFlags);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mVisits.Clear();

  for (const URITitle& title : mTitles) {
    rv = aHistory->SetURITitle(title.mURI, title.mTitle);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mTitles.Clear();

  return NS_OK;
}

nsresult
PendingGlobalHistoryEntry::ApplyChanges(nsIGlobalHistory2* aHistory)
{
  nsresult rv;
  for (const URIVisit& visit : mVisits) {
    bool redirect = (visit.mFlags & IHistory::REDIRECT_TEMPORARY) ||
      (visit.mFlags & IHistory::REDIRECT_PERMANENT);
    bool toplevel = (visit.mFlags & IHistory::TOP_LEVEL);

    rv = aHistory->AddURI(visit.mURI, redirect, toplevel, visit.mReferrerURI);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mVisits.Clear();

  for (const URITitle& title : mTitles) {
    rv = aHistory->SetPageTitle(title.mURI, title.mTitle);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mTitles.Clear();

  return NS_OK;
}

} // namespace dom

} // namespace mozilla
