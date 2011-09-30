/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Boris Zbarsky <bzbarsky@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
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

/*
 * representation of media lists used when linking to style sheets or by
 * @media rules
 */

#ifndef nsIMediaList_h_
#define nsIMediaList_h_

#include "nsIDOMMediaList.h"
#include "nsTArray.h"
#include "nsIAtom.h"
#include "nsCSSValue.h"

class nsPresContext;
class nsCSSStyleSheet;
class nsAString;
struct nsMediaFeature;

struct nsMediaExpression {
  enum Range { eMin, eMax, eEqual };

  const nsMediaFeature *mFeature;
  Range mRange;
  nsCSSValue mValue;

  // aActualValue must be obtained from mFeature->mGetter
  bool Matches(nsPresContext* aPresContext,
                 const nsCSSValue& aActualValue) const;
};

/**
 * An nsMediaQueryResultCacheKey records what feature/value combinations
 * a set of media query results are valid for.  This allows the caller
 * to quickly learn whether a prior result of media query evaluation is
 * still valid (e.g., due to a window size change) without rerunning all
 * of the evaluation and rebuilding the list of rules.
 *
 * This object may not be used after any media rules in any of the
 * sheets it was given to have been modified.  However, this is
 * generally not a problem since ClearRuleCascades is called on the
 * sheet whenever this happens, and these objects are stored inside the
 * rule cascades.  (FIXME: We're not actually doing this all the time.)
 *
 * The implementation could be further optimized in the future to store
 * ranges (combinations of less-than, less-than-or-equal, greater-than,
 * greater-than-or-equal, equal, not-equal, present, not-present) for
 * each feature rather than simply storing the list of expressions.
 * However, this requires combining any such ranges.
 */
class nsMediaQueryResultCacheKey {
public:
  nsMediaQueryResultCacheKey(nsIAtom* aMedium)
    : mMedium(aMedium)
  {}

  /**
   * Record that aExpression was tested while building the cached set
   * that this cache key is for, and that aExpressionMatches was whether
   * it matched.
   */
  void AddExpression(const nsMediaExpression* aExpression,
                     bool aExpressionMatches);
  bool Matches(nsPresContext* aPresContext) const;
private:
  struct ExpressionEntry {
    // FIXME: if we were better at maintaining invariants about clearing
    // rule cascades when media lists change, this could be a |const
    // nsMediaExpression*| instead.
    nsMediaExpression mExpression;
    bool mExpressionMatches;
  };
  struct FeatureEntry {
    const nsMediaFeature *mFeature;
    nsTArray<ExpressionEntry> mExpressions;
  };
  nsCOMPtr<nsIAtom> mMedium;
  nsTArray<FeatureEntry> mFeatureCache;
};

class nsMediaQuery {
public:
  nsMediaQuery()
    : mNegated(PR_FALSE)
    , mHasOnly(PR_FALSE)
    , mTypeOmitted(PR_FALSE)
    , mHadUnknownExpression(PR_FALSE)
  {
  }

private:
  // for Clone only
  nsMediaQuery(const nsMediaQuery& aOther)
    : mNegated(aOther.mNegated)
    , mHasOnly(aOther.mHasOnly)
    , mTypeOmitted(aOther.mTypeOmitted)
    , mHadUnknownExpression(aOther.mHadUnknownExpression)
    , mMediaType(aOther.mMediaType)
    // Clone checks the result of this deep copy for allocation failure
    , mExpressions(aOther.mExpressions)
  {
  }

public:

  void SetNegated()                     { mNegated = PR_TRUE; }
  void SetHasOnly()                     { mHasOnly = PR_TRUE; }
  void SetTypeOmitted()                 { mTypeOmitted = PR_TRUE; }
  void SetHadUnknownExpression()        { mHadUnknownExpression = PR_TRUE; }
  void SetType(nsIAtom* aMediaType)     { 
                                          NS_ASSERTION(aMediaType,
                                                       "expected non-null");
                                          mMediaType = aMediaType;
                                        }

  // Return a new nsMediaExpression in the array for the caller to fill
  // in.  The caller must either fill it in completely, or call
  // SetHadUnknownExpression on this nsMediaQuery.
  // Returns null on out-of-memory.
  nsMediaExpression* NewExpression()    { return mExpressions.AppendElement(); }

  void AppendToString(nsAString& aString) const;

  nsMediaQuery* Clone() const;

  // Does this query apply to the presentation?
  // If |aKey| is non-null, add cache information to it.
  bool Matches(nsPresContext* aPresContext,
                 nsMediaQueryResultCacheKey* aKey) const;

private:
  bool mNegated;
  bool mHasOnly; // only needed for serialization
  bool mTypeOmitted; // only needed for serialization
  bool mHadUnknownExpression;
  nsCOMPtr<nsIAtom> mMediaType;
  nsTArray<nsMediaExpression> mExpressions;
};

class nsMediaList : public nsIDOMMediaList {
public:
  nsMediaList();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMMEDIALIST

  nsresult GetText(nsAString& aMediaText);
  nsresult SetText(const nsAString& aMediaText);

  // Does this query apply to the presentation?
  // If |aKey| is non-null, add cache information to it.
  bool Matches(nsPresContext* aPresContext,
                 nsMediaQueryResultCacheKey* aKey);

  nsresult SetStyleSheet(nsCSSStyleSheet* aSheet);
  nsresult AppendQuery(nsAutoPtr<nsMediaQuery>& aQuery) {
    // Takes ownership of aQuery (if it succeeds)
    if (!mArray.AppendElement(aQuery.get())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aQuery.forget();
    return NS_OK;
  }

  nsresult Clone(nsMediaList** aResult);

  PRInt32 Count() { return mArray.Length(); }
  nsMediaQuery* MediumAt(PRInt32 aIndex) { return mArray[aIndex]; }
  void Clear() { mArray.Clear(); }

protected:
  ~nsMediaList();

  nsresult Delete(const nsAString & aOldMedium);
  nsresult Append(const nsAString & aOldMedium);

  nsTArray<nsAutoPtr<nsMediaQuery> > mArray;
  // not refcounted; sheet will let us know when it goes away
  // mStyleSheet is the sheet that needs to be dirtied when this medialist
  // changes
  nsCSSStyleSheet*         mStyleSheet;
};
#endif /* !defined(nsIMediaList_h_) */
