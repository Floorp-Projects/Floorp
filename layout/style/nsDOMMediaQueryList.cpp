/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is nsDOMMediaQueryList.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    mMatchesValid(PR_FALSE)
{
  PR_INIT_CLIST(this);

  nsCSSParser parser;
  parser.ParseMediaList(aMediaQueryList, nsnull, 0, mMediaList, PR_FALSE);
}

nsDOMMediaQueryList::~nsDOMMediaQueryList()
{
  if (mPresContext) {
    PR_REMOVE_LINK(this);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMMediaQueryList)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMMediaQueryList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPresContext)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSTARRAY_OF_NSCOMPTR(mListeners)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMMediaQueryList)
if (tmp->mPresContext) {
  PR_REMOVE_LINK(tmp);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mPresContext)
}
NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mListeners)
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
nsDOMMediaQueryList::GetMatches(PRBool *aMatches)
{
  if (!mMatchesValid) {
    NS_ABORT_IF_FALSE(mListeners.Length() == 0,
                      "when listeners present, must keep mMatches current");
    RecomputeMatches();
  }

  *aMatches = mMatches;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMediaQueryList::AddListener(nsIDOMMediaQueryListListener *aListener)
{
  if (!mMatchesValid) {
    NS_ABORT_IF_FALSE(mListeners.Length() == 0,
                      "when listeners present, must keep mMatches current");
    RecomputeMatches();
  }

  if (!mListeners.Contains(aListener)) {
    mListeners.AppendElement(aListener);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMediaQueryList::RemoveListener(nsIDOMMediaQueryListListener *aListener)
{
  mListeners.RemoveElement(aListener);
  NS_ABORT_IF_FALSE(!mListeners.Contains(aListener),
                    "duplicate occurrence of listeners");
  return NS_OK;
}

void
nsDOMMediaQueryList::RecomputeMatches()
{
  if (!mPresContext) {
    return;
  }

  mMatches = mMediaList->Matches(mPresContext, nsnull);
  mMatchesValid = PR_TRUE;
}

void
nsDOMMediaQueryList::MediumFeaturesChanged(NotifyList &aListenersToNotify)
{
  mMatchesValid = PR_FALSE;

  if (mListeners.Length()) {
    PRPackedBool oldMatches = mMatches;
    RecomputeMatches();
    if (mMatches != oldMatches) {
      for (PRUint32 i = 0, i_end = mListeners.Length(); i != i_end; ++i) {
        HandleChangeData *d = aListenersToNotify.AppendElement();
        if (d) {
          d->mql = this;
          d->listener = mListeners[i];
        }
      }
    }
  }
}
