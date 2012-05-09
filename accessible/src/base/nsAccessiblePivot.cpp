/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Eitan Isaacson <eitan@monotonous.org> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsAccessiblePivot.h"

#include "Accessible-inl.h"
#include "nsAccUtils.h"
#include "nsHyperTextAccessible.h"
#include "States.h"

#include "nsArrayUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsISupportsPrimitives.h"

using namespace mozilla::a11y;


/**
 * An object that stores a given traversal rule during 
 */
class RuleCache
{
public:
  RuleCache(nsIAccessibleTraversalRule* aRule) : mRule(aRule),
                                                 mAcceptRoles(nsnull) { }
  ~RuleCache () {
    if (mAcceptRoles)
      nsMemory::Free(mAcceptRoles);
  }

  nsresult ApplyFilter(nsAccessible* aAccessible, PRUint16* aResult);

private:
  nsCOMPtr<nsIAccessibleTraversalRule> mRule;
  PRUint32* mAcceptRoles;
  PRUint32 mAcceptRolesLength;
  PRUint32 mPreFilter;
};

////////////////////////////////////////////////////////////////////////////////
// nsAccessiblePivot

nsAccessiblePivot::nsAccessiblePivot(nsAccessible* aRoot) :
  mRoot(aRoot), mPosition(nsnull),
  mStartOffset(-1), mEndOffset(-1)
{
  NS_ASSERTION(aRoot, "A root accessible is required");
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_CYCLE_COLLECTION_CLASS(nsAccessiblePivot)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsAccessiblePivot)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mRoot, nsIAccessible)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mPosition, nsIAccessible)
  PRUint32 i, length = tmp->mObservers.Length();                               \
  for (i = 0; i < length; ++i) {
    cb.NoteXPCOMChild(tmp->mObservers.ElementAt(i).get());
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsAccessiblePivot)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRoot)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mPosition)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSTARRAY(mObservers)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsAccessiblePivot)
  NS_INTERFACE_MAP_ENTRY(nsIAccessiblePivot)
  NS_INTERFACE_MAP_ENTRY(nsAccessiblePivot)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAccessiblePivot)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsAccessiblePivot)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsAccessiblePivot)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessiblePivot

NS_IMETHODIMP
nsAccessiblePivot::GetRoot(nsIAccessible** aRoot)
{
  NS_ENSURE_ARG_POINTER(aRoot);

  NS_IF_ADDREF(*aRoot = mRoot);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::GetPosition(nsIAccessible** aPosition)
{
  NS_ENSURE_ARG_POINTER(aPosition);

  NS_IF_ADDREF(*aPosition = mPosition);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::SetPosition(nsIAccessible* aPosition)
{
  nsRefPtr<nsAccessible> secondPosition;

  if (aPosition) {
    secondPosition = do_QueryObject(aPosition);
    if (!secondPosition || !IsRootDescendant(secondPosition))
      return NS_ERROR_INVALID_ARG;
  }

  // Swap old position with new position, saves us an AddRef/Release.
  mPosition.swap(secondPosition);
  PRInt32 oldStart = mStartOffset, oldEnd = mEndOffset;
  mStartOffset = mEndOffset = -1;
  NotifyPivotChanged(secondPosition, oldStart, oldEnd);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::GetStartOffset(PRInt32* aStartOffset)
{
  NS_ENSURE_ARG_POINTER(aStartOffset);

  *aStartOffset = mStartOffset;

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::GetEndOffset(PRInt32* aEndOffset)
{
  NS_ENSURE_ARG_POINTER(aEndOffset);

  *aEndOffset = mEndOffset;

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::SetTextRange(nsIAccessibleText* aTextAccessible,
                                PRInt32 aStartOffset, PRInt32 aEndOffset)
{
  NS_ENSURE_ARG(aTextAccessible);

  // Check that start offset is smaller than end offset, and that if a value is
  // smaller than 0, both should be -1.
  NS_ENSURE_TRUE(aStartOffset <= aEndOffset &&
                 (aStartOffset >= 0 || (aStartOffset != -1 && aEndOffset != -1)),
                 NS_ERROR_INVALID_ARG);

  nsRefPtr<nsHyperTextAccessible> newPosition = do_QueryObject(aTextAccessible);
  if (!newPosition || !IsRootDescendant(newPosition))
    return NS_ERROR_INVALID_ARG;

  // Make sure the given offsets don't exceed the character count.
  PRInt32 charCount = newPosition->CharacterCount();

  if (aEndOffset > charCount)
    return NS_ERROR_FAILURE;

  PRInt32 oldStart = mStartOffset, oldEnd = mEndOffset;
  mStartOffset = aStartOffset;
  mEndOffset = aEndOffset;

  nsRefPtr<nsAccessible> oldPosition = mPosition.forget();
  mPosition = newPosition.forget();

  NotifyPivotChanged(oldPosition, oldStart, oldEnd);

  return NS_OK;
}

// Traversal functions

NS_IMETHODIMP
nsAccessiblePivot::MoveNext(nsIAccessibleTraversalRule* aRule, bool* aResult)
{
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  nsresult rv = NS_OK;
  nsAccessible* accessible = SearchForward(mPosition, aRule, false, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = accessible;
  if (*aResult)
    MovePivotInternal(accessible);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MovePrevious(nsIAccessibleTraversalRule* aRule, bool* aResult)
{
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  nsresult rv = NS_OK;
  nsAccessible* accessible = SearchBackward(mPosition, aRule, false, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = accessible;
  if (*aResult)
    MovePivotInternal(accessible);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MoveFirst(nsIAccessibleTraversalRule* aRule, bool* aResult)
{
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);
  nsresult rv = NS_OK;
  nsAccessible* accessible = SearchForward(mRoot, aRule, true, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = accessible;
  if (*aResult)
    MovePivotInternal(accessible);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MoveLast(nsIAccessibleTraversalRule* aRule, bool* aResult)
{
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  *aResult = false;
  nsresult rv = NS_OK;
  nsAccessible* lastAccessible = mRoot;
  nsAccessible* accessible = nsnull;

  // First got to the last accessible in pre-order
  while (lastAccessible->HasChildren())
    lastAccessible = lastAccessible->LastChild();

  // Search backwards from last accessible and find the last occurrence in the doc
  accessible = SearchBackward(lastAccessible, aRule, true, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = accessible;
  if (*aResult)
    MovePivotInternal(accessible);

  return NS_OK;
}

// TODO: Implement
NS_IMETHODIMP
nsAccessiblePivot::MoveNextByText(TextBoundaryType aBoundary, bool* aResult)
{
  NS_ENSURE_ARG(aResult);

  *aResult = false;

  return NS_ERROR_NOT_IMPLEMENTED;
}

// TODO: Implement
NS_IMETHODIMP
nsAccessiblePivot::MovePreviousByText(TextBoundaryType aBoundary, bool* aResult)
{
  NS_ENSURE_ARG(aResult);

  *aResult = false;

  return NS_ERROR_NOT_IMPLEMENTED;
}

// Observer functions

NS_IMETHODIMP
nsAccessiblePivot::AddObserver(nsIAccessiblePivotObserver* aObserver)
{
  NS_ENSURE_ARG(aObserver);

  mObservers.AppendElement(aObserver);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::RemoveObserver(nsIAccessiblePivotObserver* aObserver)
{
  NS_ENSURE_ARG(aObserver);

  return mObservers.RemoveElement(aObserver) ? NS_OK : NS_ERROR_FAILURE;
}

// Private utility methods

bool
nsAccessiblePivot::IsRootDescendant(nsAccessible* aAccessible)
{
  nsAccessible* accessible = aAccessible;
  do {
    if (accessible == mRoot)
      return true;
  } while ((accessible = accessible->Parent()));

  return false;
}

void
nsAccessiblePivot::MovePivotInternal(nsAccessible* aPosition)
{
  nsRefPtr<nsAccessible> oldPosition = mPosition.forget();
  mPosition = aPosition;
  PRInt32 oldStart = mStartOffset, oldEnd = mEndOffset;
  mStartOffset = mEndOffset = -1;

  NotifyPivotChanged(oldPosition, oldStart, oldEnd);
}

nsAccessible*
nsAccessiblePivot::SearchBackward(nsAccessible* aAccessible,
                                  nsIAccessibleTraversalRule* aRule,
                                  bool searchCurrent,
                                  nsresult* rv)
{
  *rv = NS_OK;

  // Initial position could be unset, in that case return null.
  if (!aAccessible)
    return nsnull;

  RuleCache cache(aRule);
  nsAccessible* accessible = aAccessible;

  PRUint16 filtered = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (searchCurrent) {
    *rv = cache.ApplyFilter(accessible, &filtered);
    NS_ENSURE_SUCCESS(*rv, nsnull);
    if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)
      return accessible;
  }

  while (accessible != mRoot) {
    nsAccessible* parent = accessible->Parent();
    PRInt32 idxInParent = accessible->IndexInParent();
    while (idxInParent > 0) {
      if (!(accessible = parent->GetChildAt(--idxInParent)))
        continue;

      *rv = cache.ApplyFilter(accessible, &filtered);
      NS_ENSURE_SUCCESS(*rv, nsnull);

      nsAccessible* lastChild;
      while (!(filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) &&
             (lastChild = accessible->LastChild())) {
        parent = accessible;
        accessible = lastChild;
        *rv = cache.ApplyFilter(accessible, &filtered);
        NS_ENSURE_SUCCESS(*rv, nsnull);
      }

      if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)
        return accessible;
    }

    if (!(accessible = parent))
      break;

    *rv = cache.ApplyFilter(accessible, &filtered);
    NS_ENSURE_SUCCESS(*rv, nsnull);

    if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)
      return accessible;
  }

  return nsnull;
}

nsAccessible*
nsAccessiblePivot::SearchForward(nsAccessible* aAccessible,
                                 nsIAccessibleTraversalRule* aRule,
                                 bool searchCurrent,
                                 nsresult* rv)
{
  *rv = NS_OK;

  // Initial position could be not set, in that case begin search from root.
  nsAccessible *accessible = (!aAccessible) ? mRoot.get() : aAccessible;

  RuleCache cache(aRule);

  PRUint16 filtered = nsIAccessibleTraversalRule::FILTER_IGNORE;
  *rv = cache.ApplyFilter(accessible, &filtered);
  NS_ENSURE_SUCCESS(*rv, nsnull);
  if (searchCurrent && (filtered & nsIAccessibleTraversalRule::FILTER_MATCH))
    return accessible;

  while (true) {
    nsAccessible* firstChild = nsnull;
    while (!(filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) &&
           (firstChild = accessible->FirstChild())) {
      accessible = firstChild;
      *rv = cache.ApplyFilter(accessible, &filtered);
      NS_ENSURE_SUCCESS(*rv, nsnull);

      if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)
        return accessible;
    }

    nsAccessible* sibling = nsnull;
    nsAccessible* temp = accessible;
    do {
      if (temp == mRoot)
        break;

      sibling = temp->NextSibling();

      if (sibling)
        break;
    } while ((temp = temp->Parent()));

    if (!sibling)
      break;

    accessible = sibling;
    *rv = cache.ApplyFilter(accessible, &filtered);
    NS_ENSURE_SUCCESS(*rv, nsnull);

    if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)
      return accessible;
  }

  return nsnull;
}

void
nsAccessiblePivot::NotifyPivotChanged(nsAccessible* aOldPosition,
                                      PRInt32 aOldStart, PRInt32 aOldEnd)
{
  nsTObserverArray<nsCOMPtr<nsIAccessiblePivotObserver> >::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    nsIAccessiblePivotObserver* obs = iter.GetNext();
    obs->OnPivotChanged(this, aOldPosition, aOldStart, aOldEnd);
  }
}

nsresult
RuleCache::ApplyFilter(nsAccessible* aAccessible, PRUint16* aResult)
{
  *aResult = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (!mAcceptRoles) {
    nsresult rv = mRule->GetMatchRoles(&mAcceptRoles, &mAcceptRolesLength);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mRule->GetPreFilter(&mPreFilter);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mPreFilter) {
    PRUint64 state = aAccessible->State();

    if ((nsIAccessibleTraversalRule::PREFILTER_INVISIBLE & mPreFilter) &&
        (state & states::INVISIBLE))
      return NS_OK;

    if ((nsIAccessibleTraversalRule::PREFILTER_OFFSCREEN & mPreFilter) &&
        (state & states::OFFSCREEN))
      return NS_OK;

    if ((nsIAccessibleTraversalRule::PREFILTER_NOT_FOCUSABLE & mPreFilter) &&
        !(state & states::FOCUSABLE))
      return NS_OK;
  }

  if (mAcceptRolesLength > 0) {
    PRUint32 accessibleRole = aAccessible->Role();
    bool matchesRole = false;
    for (PRUint32 idx = 0; idx < mAcceptRolesLength; idx++) {
      matchesRole = mAcceptRoles[idx] == accessibleRole;
      if (matchesRole)
        break;
    }
    if (!matchesRole)
      return NS_OK;
  }

  return mRule->Match(aAccessible, aResult);
}
