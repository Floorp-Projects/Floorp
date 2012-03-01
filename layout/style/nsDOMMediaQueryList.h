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

#ifndef nsDOMMediaQueryList_h_
#define nsDOMMediaQueryList_h_

#include "nsIDOMMediaQueryList.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "prclist.h"

class nsPresContext;
class nsMediaList;

class nsDOMMediaQueryList : public nsIDOMMediaQueryList,
                            public PRCList
{
public:
  // The caller who constructs is responsible for calling Evaluate
  // before calling any other methods.
  nsDOMMediaQueryList(nsPresContext *aPresContext,
                      const nsAString &aMediaQueryList);
private:
  ~nsDOMMediaQueryList();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMMediaQueryList)

  NS_DECL_NSIDOMMEDIAQUERYLIST

  struct HandleChangeData {
    nsRefPtr<nsDOMMediaQueryList> mql;
    nsCOMPtr<nsIDOMMediaQueryListListener> listener;
  };

  typedef FallibleTArray< nsCOMPtr<nsIDOMMediaQueryListListener> > ListenerList;
  typedef FallibleTArray<HandleChangeData> NotifyList;

  // Appends listeners that need notification to aListenersToNotify
  void MediumFeaturesChanged(NotifyList &aListenersToNotify);

  bool HasListeners() const { return !mListeners.IsEmpty(); }

  void RemoveAllListeners();

private:
  void RecomputeMatches();

  // We only need a pointer to the pres context to support lazy
  // reevaluation following dynamic changes.  However, this lazy
  // reevaluation is perhaps somewhat important, since some usage
  // patterns may involve the creation of large numbers of
  // MediaQueryList objects which almost immediately become garbage
  // (after a single call to the .matches getter).
  //
  // This pointer does make us a little more dependent on cycle
  // collection.
  //
  // We have a non-null mPresContext for our entire lifetime except
  // after cycle collection unlinking.  Having a non-null mPresContext
  // is equivalent to being in that pres context's mDOMMediaQueryLists
  // linked list.
  nsRefPtr<nsPresContext> mPresContext;

  nsRefPtr<nsMediaList> mMediaList;
  bool mMatches;
  bool mMatchesValid;
  ListenerList mListeners;
};

#endif /* !defined(nsDOMMediaQueryList_h_) */
