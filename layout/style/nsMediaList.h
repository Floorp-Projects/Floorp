/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * representation of media lists used when linking to style sheets or by
 * @media rules
 */

#ifndef nsMediaList_h_
#define nsMediaList_h_

#include "nsAutoPtr.h"
#include "nsIDOMMediaList.h"
#include "nsTArray.h"
#include "nsIAtom.h"
#include "nsCSSValue.h"
#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"

class nsPresContext;
class nsAString;
struct nsMediaFeature;

namespace mozilla {
class StyleSheet;
namespace css {
class DocumentRule;
} // namespace css
} // namespace mozilla

struct nsMediaExpression {
  enum Range { eMin, eMax, eEqual };

  const nsMediaFeature *mFeature;
  Range mRange;
  nsCSSValue mValue;

  // aActualValue must be obtained from mFeature->mGetter
  bool Matches(nsPresContext* aPresContext,
               const nsCSSValue& aActualValue) const;

  bool operator==(const nsMediaExpression& aOther) const {
    return mFeature == aOther.mFeature && // pointer equality fine (atom-like)
           mRange == aOther.mRange &&
           mValue == aOther.mValue;
  }
  bool operator!=(const nsMediaExpression& aOther) const {
    return !(*this == aOther);
  }
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
  explicit nsMediaQueryResultCacheKey(nsIAtom* aMedium)
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
  bool HasFeatureConditions() const {
    return !mFeatureCache.IsEmpty();
  }

  /**
   * An operator== that implements list equality, which isn't quite as
   * good as set equality, but catches the trivial equality cases.
   */
  bool operator==(const nsMediaQueryResultCacheKey& aOther) const {
    return mMedium == aOther.mMedium &&
           mFeatureCache == aOther.mFeatureCache;
  }
  bool operator!=(const nsMediaQueryResultCacheKey& aOther) const {
    return !(*this == aOther);
  }
private:
  struct ExpressionEntry {
    // FIXME: if we were better at maintaining invariants about clearing
    // rule cascades when media lists change, this could be a |const
    // nsMediaExpression*| instead.
    nsMediaExpression mExpression;
    bool mExpressionMatches;

    bool operator==(const ExpressionEntry& aOther) const {
      return mExpression == aOther.mExpression &&
             mExpressionMatches == aOther.mExpressionMatches;
    }
    bool operator!=(const ExpressionEntry& aOther) const {
      return !(*this == aOther);
    }
  };
  struct FeatureEntry {
    const nsMediaFeature *mFeature;
    InfallibleTArray<ExpressionEntry> mExpressions;

    bool operator==(const FeatureEntry& aOther) const {
      return mFeature == aOther.mFeature &&
             mExpressions == aOther.mExpressions;
    }
    bool operator!=(const FeatureEntry& aOther) const {
      return !(*this == aOther);
    }
  };
  nsCOMPtr<nsIAtom> mMedium;
  nsTArray<FeatureEntry> mFeatureCache;
};

/**
 * nsDocumentRuleResultCacheKey is analagous to nsMediaQueryResultCacheKey
 * and stores the result of matching the @-moz-document rules from a set
 * of style sheets.  nsCSSRuleProcessor builds up an
 * nsDocumentRuleResultCacheKey as it visits the @-moz-document rules
 * while building its RuleCascadeData.
 *
 * Rather than represent the result using a list of both the matching and
 * non-matching rules, we just store the matched rules.  The assumption is
 * that in situations where we have a large number of rules -- such as the
 * thousands added by AdBlock Plus -- that only a small number will be
 * matched.  Thus to check if the nsDocumentRuleResultCacheKey matches a
 * given nsPresContext, we also need the entire list of @-moz-document
 * rules to know which rules must not match.
 */
class nsDocumentRuleResultCacheKey
{
public:
#ifdef DEBUG
  nsDocumentRuleResultCacheKey()
    : mFinalized(false) {}
#endif

  bool AddMatchingRule(mozilla::css::DocumentRule* aRule);
  bool Matches(nsPresContext* aPresContext,
               const nsTArray<mozilla::css::DocumentRule*>& aRules) const;

  bool operator==(const nsDocumentRuleResultCacheKey& aOther) const {
    MOZ_ASSERT(mFinalized);
    MOZ_ASSERT(aOther.mFinalized);
    return mMatchingRules == aOther.mMatchingRules;
  }
  bool operator!=(const nsDocumentRuleResultCacheKey& aOther) const {
    return !(*this == aOther);
  }

  void Swap(nsDocumentRuleResultCacheKey& aOther) {
    mMatchingRules.SwapElements(aOther.mMatchingRules);
#ifdef DEBUG
    std::swap(mFinalized, aOther.mFinalized);
#endif
  }

  void Finalize();

#ifdef DEBUG
  void List(FILE* aOut = stdout, int32_t aIndex = 0) const;
#endif

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  nsTArray<mozilla::css::DocumentRule*> mMatchingRules;
#ifdef DEBUG
  bool mFinalized;
#endif
};

class nsMediaQuery {
public:
  nsMediaQuery()
    : mNegated(false)
    , mHasOnly(false)
    , mTypeOmitted(false)
    , mHadUnknownExpression(false)
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
    , mExpressions(aOther.mExpressions)
  {
    MOZ_ASSERT(mExpressions.Length() == aOther.mExpressions.Length());
  }

public:

  void SetNegated()                     { mNegated = true; }
  void SetHasOnly()                     { mHasOnly = true; }
  void SetTypeOmitted()                 { mTypeOmitted = true; }
  void SetHadUnknownExpression()        { mHadUnknownExpression = true; }
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

class nsMediaList final : public nsIDOMMediaList
                        , public nsWrapperCache
{
public:
  typedef mozilla::ErrorResult ErrorResult;

  nsMediaList();

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
  nsISupports* GetParentObject() const
  {
    return nullptr;
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsMediaList)

  NS_DECL_NSIDOMMEDIALIST

  void GetText(nsAString& aMediaText);
  void SetText(const nsAString& aMediaText);

  // Does this query apply to the presentation?
  // If |aKey| is non-null, add cache information to it.
  bool Matches(nsPresContext* aPresContext,
                 nsMediaQueryResultCacheKey* aKey);

  void SetStyleSheet(mozilla::StyleSheet* aSheet);
  void AppendQuery(nsAutoPtr<nsMediaQuery>& aQuery) {
    // Takes ownership of aQuery
    mArray.AppendElement(aQuery.forget());
  }

  already_AddRefed<nsMediaList> Clone();

  nsMediaQuery* MediumAt(int32_t aIndex) { return mArray[aIndex]; }
  void Clear() { mArray.Clear(); }

  // WebIDL
  // XPCOM GetMediaText and SetMediaText are fine.
  uint32_t Length() { return mArray.Length(); }
  void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aReturn);
  // XPCOM Item is fine.
  void DeleteMedium(const nsAString& aMedium, ErrorResult& aRv)
  {
    aRv = DeleteMedium(aMedium);
  }
  void AppendMedium(const nsAString& aMedium, ErrorResult& aRv)
  {
    aRv = AppendMedium(aMedium);
  }

protected:
  ~nsMediaList();

  template<typename Func>
  nsresult DoMediaChange(Func aCallback);

  nsresult Delete(const nsAString & aOldMedium);
  nsresult Append(const nsAString & aOldMedium);

  InfallibleTArray<nsAutoPtr<nsMediaQuery> > mArray;
  // not refcounted; sheet will let us know when it goes away
  // mStyleSheet is the sheet that needs to be dirtied when this medialist
  // changes
  mozilla::StyleSheet* mStyleSheet;
};
#endif /* !defined(nsMediaList_h_) */
