/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implements DOM interface for querying and observing media queries */

#include "nsDOMMediaQueryList.h"
#include "nsPresContext.h"
#include "nsIMediaList.h"
#include "nsCSSParser.h"
#include "nsDOMClassInfoID.h" // DOMCI_DATA

nsDOMMediaQueryList::nsDOMMediaQueryList(nsPresContext *aPresContext,
                                         const nsAString &aMediaQueryList)
  : mPresContext(aPresContext),
    mMediaList(new nsMediaList),
    mMatchesValid(false)
{
  PR_INIT_CLIST(this);

  nsCSSParser parser;
  parser.ParseMediaList(aMediaQueryList, nullptr, 0, mMediaList, false);
}

nsDOMMediaQueryList::~nsDOMMediaQueryList()
{
  if (mPresContext) {
    PR_REMOVE_LINK(this);
  }
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMMediaQueryList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPresContext)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListeners)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMMediaQueryList)
if (tmp->mPresContext) {
  PR_REMOVE_LINK(tmp);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPresContext)
}
tmp->RemoveAllListeners();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

DOMCI_DATA(MediaQueryList, nsDOMMediaQueryList)

NS_INTERFACE_MAP_BEGIN(nsDOMMediaQueryList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMediaQueryList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsDOMMediaQueryList)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MediaQueryList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMMediaQueryList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMMediaQueryList)

NS_IMETHODIMP
nsDOMMediaQueryList::GetMedia(nsAString &aMedia)
{
  mMediaList->GetText(aMedia);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMediaQueryList::GetMatches(bool *aMatches)
{
  if (!mMatchesValid) {
    NS_ABORT_IF_FALSE(!HasListeners(),
                      "when listeners present, must keep mMatches current");
    RecomputeMatches();
  }

  *aMatches = mMatches;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMediaQueryList::AddListener(nsIDOMMediaQueryListListener *aListener)
{
  if (!aListener) {
    return NS_OK;
  }

  if (!HasListeners()) {
    // When we have listeners, the pres context owns a reference to
    // this.  This is a cyclic reference that can only be broken by
    // cycle collection.
    NS_ADDREF_THIS();
  }

  if (!mMatchesValid) {
    NS_ABORT_IF_FALSE(!HasListeners(),
                      "when listeners present, must keep mMatches current");
    RecomputeMatches();
  }

  if (!mListeners.Contains(aListener)) {
    mListeners.AppendElement(aListener);
    if (!HasListeners()) {
      // Append failed; undo the AddRef above.
      NS_RELEASE_THIS();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMediaQueryList::RemoveListener(nsIDOMMediaQueryListListener *aListener)
{
  bool removed = mListeners.RemoveElement(aListener);
  NS_ABORT_IF_FALSE(!mListeners.Contains(aListener),
                    "duplicate occurrence of listeners");

  if (removed && !HasListeners()) {
    // See NS_ADDREF_THIS() in AddListener.
    NS_RELEASE_THIS();
  }

  return NS_OK;
}

void
nsDOMMediaQueryList::RemoveAllListeners()
{
  bool hadListeners = HasListeners();
  mListeners.Clear();
  if (hadListeners) {
    // See NS_ADDREF_THIS() in AddListener.
    NS_RELEASE_THIS();
  }
}

void
nsDOMMediaQueryList::RecomputeMatches()
{
  if (!mPresContext) {
    return;
  }

  mMatches = mMediaList->Matches(mPresContext, nullptr);
  mMatchesValid = true;
}

void
nsDOMMediaQueryList::MediumFeaturesChanged(NotifyList &aListenersToNotify)
{
  mMatchesValid = false;

  if (mListeners.Length()) {
    bool oldMatches = mMatches;
    RecomputeMatches();
    if (mMatches != oldMatches) {
      for (uint32_t i = 0, i_end = mListeners.Length(); i != i_end; ++i) {
        HandleChangeData *d = aListenersToNotify.AppendElement();
        if (d) {
          d->mql = this;
          d->listener = mListeners[i];
        }
      }
    }
  }
}
