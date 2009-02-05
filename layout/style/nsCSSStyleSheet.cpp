/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=2:expandtab:shiftwidth=2:
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Daniel Glazman <glazman@netscape.com>
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

/* representation of a CSS style sheet */

#include "nsCSSStyleSheet.h"

#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIServiceManager.h"
#include "nsCSSRuleProcessor.h"
#include "nsICSSStyleRule.h"
#include "nsICSSNameSpaceRule.h"
#include "nsICSSGroupRule.h"
#include "nsICSSImportRule.h"
#include "nsIMediaList.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIDOMStyleSheetList.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsIDOMCSSRule.h"
#include "nsIDOMCSSImportRule.h"
#include "nsICSSRuleList.h"
#include "nsIDOMMediaList.h"
#include "nsIDOMNode.h"
#include "nsDOMError.h"
#include "nsICSSParser.h"
#include "nsICSSLoader.h"
#include "nsICSSLoaderObserver.h"
#include "nsINameSpaceManager.h"
#include "nsXMLNameSpaceMap.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "mozAutoDocUpdate.h"
#include "nsCSSDeclaration.h"
#include "nsRuleNode.h"

// -------------------------------
// Style Rule List for the DOM
//
class CSSRuleListImpl : public nsICSSRuleList
{
public:
  CSSRuleListImpl(nsCSSStyleSheet *aStyleSheet);

  NS_DECL_ISUPPORTS

  // nsIDOMCSSRuleList interface
  NS_IMETHOD    GetLength(PRUint32* aLength); 
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMCSSRule** aReturn); 

  virtual nsIDOMCSSRule* GetItemAt(PRUint32 aIndex, nsresult* aResult);

  void DropReference() { mStyleSheet = nsnull; }

protected:
  virtual ~CSSRuleListImpl();

  nsCSSStyleSheet*  mStyleSheet;
public:
  PRBool              mRulesAccessed;
};

CSSRuleListImpl::CSSRuleListImpl(nsCSSStyleSheet *aStyleSheet)
{
  // Not reference counted to avoid circular references.
  // The style sheet will tell us when its going away.
  mStyleSheet = aStyleSheet;
  mRulesAccessed = PR_FALSE;
}

CSSRuleListImpl::~CSSRuleListImpl()
{
}

// QueryInterface implementation for CSSRuleList
NS_INTERFACE_MAP_BEGIN(CSSRuleListImpl)
  NS_INTERFACE_MAP_ENTRY(nsICSSRuleList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRuleList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSRuleList)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(CSSRuleListImpl)
NS_IMPL_RELEASE(CSSRuleListImpl)


NS_IMETHODIMP    
CSSRuleListImpl::GetLength(PRUint32* aLength)
{
  if (nsnull != mStyleSheet) {
    PRInt32 count;
    mStyleSheet->StyleRuleCount(count);
    *aLength = (PRUint32)count;
  }
  else {
    *aLength = 0;
  }

  return NS_OK;
}

nsIDOMCSSRule*    
CSSRuleListImpl::GetItemAt(PRUint32 aIndex, nsresult* aResult)
{
  nsresult result = NS_OK;

  if (mStyleSheet) {
    result = mStyleSheet->EnsureUniqueInner(); // needed to ensure rules have correct parent
    if (NS_SUCCEEDED(result)) {
      nsCOMPtr<nsICSSRule> rule;

      result = mStyleSheet->GetStyleRuleAt(aIndex, *getter_AddRefs(rule));
      if (rule) {
        mRulesAccessed = PR_TRUE; // signal to never share rules again
        return rule->GetDOMRuleWeak(aResult);
      }
      if (result == NS_ERROR_ILLEGAL_VALUE) {
        result = NS_OK; // per spec: "Return Value ... null if ... not a valid index."
      }
    }
  }

  *aResult = result;
  return nsnull;
}

NS_IMETHODIMP    
CSSRuleListImpl::Item(PRUint32 aIndex, nsIDOMCSSRule** aReturn)
{
  nsresult rv;
  nsIDOMCSSRule* rule = GetItemAt(aIndex, &rv);
  if (!rule) {
    *aReturn = nsnull;

    return rv;
  }

  return CallQueryInterface(rule, aReturn);
}

template <class Numeric>
PRInt32 DoCompare(Numeric a, Numeric b)
{
  if (a == b)
    return 0;
  if (a < b)
    return -1;
  return 1;
}

PRBool
nsMediaExpression::Matches(nsPresContext *aPresContext,
                           const nsCSSValue& aActualValue) const
{
  const nsCSSValue& actual = aActualValue;
  const nsCSSValue& required = mValue;

  // If we don't have the feature, the match fails.
  if (actual.GetUnit() == eCSSUnit_Null) {
    return PR_FALSE;
  }

  // If the expression had no value to match, the match succeeds,
  // unless the value is an integer 0 or a zero length.
  if (required.GetUnit() == eCSSUnit_Null) {
    if (actual.GetUnit() == eCSSUnit_Integer)
      return actual.GetIntValue() != 0;
    if (actual.IsLengthUnit())
      return actual.GetFloatValue() != 0;
    return PR_TRUE;
  }

  NS_ASSERTION(mFeature->mRangeType == nsMediaFeature::eMinMaxAllowed ||
               mRange == nsMediaExpression::eEqual, "yikes");
  PRInt32 cmp; // -1 (actual < required)
               //  0 (actual == required)
               //  1 (actual > required)
  switch (mFeature->mValueType) {
    case nsMediaFeature::eLength:
      {
        NS_ASSERTION(actual.IsLengthUnit(), "bad actual value");
        NS_ASSERTION(required.IsLengthUnit(), "bad required value");
        nscoord actualCoord = nsRuleNode::CalcLengthWithInitialFont(
                                aPresContext, actual);
        nscoord requiredCoord = nsRuleNode::CalcLengthWithInitialFont(
                                  aPresContext, required);
        cmp = DoCompare(actualCoord, requiredCoord);
      }
      break;
    case nsMediaFeature::eInteger:
    case nsMediaFeature::eBoolInteger:
      {
        NS_ASSERTION(actual.GetUnit() == eCSSUnit_Integer,
                     "bad actual value");
        NS_ASSERTION(required.GetUnit() == eCSSUnit_Integer,
                     "bad required value");
        NS_ASSERTION(mFeature->mValueType != nsMediaFeature::eBoolInteger ||
                     actual.GetIntValue() == 0 || actual.GetIntValue() == 1,
                     "bad actual bool integer value");
        NS_ASSERTION(mFeature->mValueType != nsMediaFeature::eBoolInteger ||
                     required.GetIntValue() == 0 || required.GetIntValue() == 1,
                     "bad required bool integer value");
        cmp = DoCompare(actual.GetIntValue(), required.GetIntValue());
      }
      break;
    case nsMediaFeature::eIntRatio:
      {
        NS_ASSERTION(actual.GetUnit() == eCSSUnit_Array &&
                     actual.GetArrayValue()->Count() == 2 &&
                     actual.GetArrayValue()->Item(0).GetUnit() ==
                       eCSSUnit_Integer &&
                     actual.GetArrayValue()->Item(1).GetUnit() ==
                       eCSSUnit_Integer,
                     "bad actual value");
        NS_ASSERTION(required.GetUnit() == eCSSUnit_Array &&
                     required.GetArrayValue()->Count() == 2 &&
                     required.GetArrayValue()->Item(0).GetUnit() ==
                       eCSSUnit_Integer &&
                     required.GetArrayValue()->Item(1).GetUnit() ==
                       eCSSUnit_Integer,
                     "bad required value");
        // Convert to PRInt64 so we can multiply without worry.  Note
        // that while the spec requires that both halves of |required|
        // be positive, the numerator or denominator of |actual| might
        // be zero (e.g., when testing 'aspect-ratio' on a 0-width or
        // 0-height iframe).
        PRInt64 actualNum = actual.GetArrayValue()->Item(0).GetIntValue(),
                actualDen = actual.GetArrayValue()->Item(1).GetIntValue(),
                requiredNum = required.GetArrayValue()->Item(0).GetIntValue(),
                requiredDen = required.GetArrayValue()->Item(1).GetIntValue();
        cmp = DoCompare(actualNum * requiredDen, requiredNum * actualDen);
      }
      break;
    case nsMediaFeature::eResolution:
      {
        NS_ASSERTION(actual.GetUnit() == eCSSUnit_Inch ||
                     actual.GetUnit() == eCSSUnit_Centimeter,
                     "bad actual value");
        NS_ASSERTION(required.GetUnit() == eCSSUnit_Inch ||
                     required.GetUnit() == eCSSUnit_Centimeter,
                     "bad required value");
        float actualDPI = actual.GetFloatValue();
        if (actual.GetUnit() == eCSSUnit_Centimeter)
          actualDPI = actualDPI * 2.54f;
        float requiredDPI = required.GetFloatValue();
        if (required.GetUnit() == eCSSUnit_Centimeter)
          requiredDPI = requiredDPI * 2.54f;
        cmp = DoCompare(actualDPI, requiredDPI);
      }
      break;
    case nsMediaFeature::eEnumerated:
      {
        NS_ASSERTION(actual.GetUnit() == eCSSUnit_Enumerated,
                     "bad actual value");
        NS_ASSERTION(required.GetUnit() == eCSSUnit_Enumerated,
                     "bad required value");
        NS_ASSERTION(mFeature->mRangeType == nsMediaFeature::eMinMaxNotAllowed,
                     "bad range"); // we asserted above about mRange
        // We don't really need DoCompare, but it doesn't hurt (and
        // maybe the compiler will condense this case with eInteger).
        cmp = DoCompare(actual.GetIntValue(), required.GetIntValue());
      }
      break;
  }
  switch (mRange) {
    case nsMediaExpression::eMin:
      return cmp != -1;
    case nsMediaExpression::eMax:
      return cmp != 1;
    case nsMediaExpression::eEqual:
      return cmp == 0;
  }
  NS_NOTREACHED("unexpected mRange");
  return PR_FALSE;
}

void
nsMediaQueryResultCacheKey::AddExpression(const nsMediaExpression* aExpression,
                                          PRBool aExpressionMatches)
{
  const nsMediaFeature *feature = aExpression->mFeature;
  FeatureEntry *entry = nsnull;
  for (PRUint32 i = 0; i < mFeatureCache.Length(); ++i) {
    if (mFeatureCache[i].mFeature == feature) {
      entry = &mFeatureCache[i];
      break;
    }
  }
  if (!entry) {
    entry = mFeatureCache.AppendElement();
    if (!entry) {
      return; /* out of memory */
    }
    entry->mFeature = feature;
  }

  ExpressionEntry eentry = { *aExpression, aExpressionMatches };
  entry->mExpressions.AppendElement(eentry);
}

PRBool
nsMediaQueryResultCacheKey::Matches(nsPresContext* aPresContext) const
{
  if (aPresContext->Medium() != mMedium) {
    return PR_FALSE;
  }

  for (PRUint32 i = 0; i < mFeatureCache.Length(); ++i) {
    const FeatureEntry *entry = &mFeatureCache[i];
    nsCSSValue actual;
    nsresult rv = (entry->mFeature->mGetter)(aPresContext, actual);
    NS_ENSURE_SUCCESS(rv, PR_FALSE); // any better ideas?

    for (PRUint32 j = 0; j < entry->mExpressions.Length(); ++j) {
      const ExpressionEntry &eentry = entry->mExpressions[j];
      if (eentry.mExpression.Matches(aPresContext, actual) !=
          eentry.mExpressionMatches) {
        return PR_FALSE;
      }
    }
  }

  return PR_TRUE;
}

void
nsMediaQuery::AppendToString(nsAString& aString) const
{
  nsAutoString buffer;

  if (mHadUnknownExpression) {
    aString.AppendLiteral("not all");
    return;
  }

  NS_ASSERTION(!mNegated || !mHasOnly, "can't have not and only");
  NS_ASSERTION(!mTypeOmitted || (!mNegated && !mHasOnly),
               "can't have not or only when type is omitted");
  if (!mTypeOmitted) {
    if (mNegated) {
      aString.AppendLiteral("not ");
    } else if (mHasOnly) {
      aString.AppendLiteral("only ");
    }
    mMediaType->ToString(buffer);
    aString.Append(buffer);
    buffer.Truncate();
  }

  for (PRUint32 i = 0, i_end = mExpressions.Length(); i < i_end; ++i) {
    if (i > 0 || !mTypeOmitted)
      aString.AppendLiteral(" and ");
    aString.AppendLiteral("(");

    const nsMediaExpression &expr = mExpressions[i];
    if (expr.mRange == nsMediaExpression::eMin) {
      aString.AppendLiteral("min-");
    } else if (expr.mRange == nsMediaExpression::eMax) {
      aString.AppendLiteral("max-");
    }

    const nsMediaFeature *feature = expr.mFeature;
    (*feature->mName)->ToString(buffer);
    aString.Append(buffer);
    buffer.Truncate();

    if (expr.mValue.GetUnit() != eCSSUnit_Null) {
      aString.AppendLiteral(": ");
      switch (feature->mValueType) {
        case nsMediaFeature::eLength:
          NS_ASSERTION(expr.mValue.IsLengthUnit(), "bad unit");
          // Use 'width' as a property that takes length values
          // written in the normal way.
          nsCSSDeclaration::AppendCSSValueToString(eCSSProperty_width,
                                                   expr.mValue, aString);
          break;
        case nsMediaFeature::eInteger:
        case nsMediaFeature::eBoolInteger:
          NS_ASSERTION(expr.mValue.GetUnit() == eCSSUnit_Integer,
                       "bad unit");
          // Use 'z-index' as a property that takes integer values
          // written without anything extra.
          nsCSSDeclaration::AppendCSSValueToString(eCSSProperty_z_index,
                                                   expr.mValue, aString);
          break;
        case nsMediaFeature::eIntRatio:
          {
            NS_ASSERTION(expr.mValue.GetUnit() == eCSSUnit_Array,
                         "bad unit");
            nsCSSValue::Array *array = expr.mValue.GetArrayValue();
            NS_ASSERTION(array->Count() == 2, "unexpected length");
            NS_ASSERTION(array->Item(0).GetUnit() == eCSSUnit_Integer,
                         "bad unit");
            NS_ASSERTION(array->Item(1).GetUnit() == eCSSUnit_Integer,
                         "bad unit");
            nsCSSDeclaration::AppendCSSValueToString(eCSSProperty_z_index,
                                                     array->Item(0), aString);
            aString.AppendLiteral("/");
            nsCSSDeclaration::AppendCSSValueToString(eCSSProperty_z_index,
                                                     array->Item(1), aString);
          }
          break;
        case nsMediaFeature::eResolution:
          buffer.AppendFloat(expr.mValue.GetFloatValue());
          aString.Append(buffer);
          buffer.Truncate();
          if (expr.mValue.GetUnit() == eCSSUnit_Inch) {
            aString.AppendLiteral("dpi");
          } else {
            NS_ASSERTION(expr.mValue.GetUnit() == eCSSUnit_Centimeter,
                         "bad unit");
            aString.AppendLiteral("dpcm");
          }
          break;
        case nsMediaFeature::eEnumerated:
          NS_ASSERTION(expr.mValue.GetUnit() == eCSSUnit_Enumerated,
                       "bad unit");
          AppendASCIItoUTF16(
              nsCSSProps::ValueToKeyword(expr.mValue.GetIntValue(),
                                         feature->mKeywordTable),
              aString);
          break;
      }
    }

    aString.AppendLiteral(")");
  }
}

nsMediaQuery*
nsMediaQuery::Clone() const
{
  nsAutoPtr<nsMediaQuery> result(new nsMediaQuery(*this));
  NS_ENSURE_TRUE(result &&
                   result->mExpressions.Length() == mExpressions.Length(),
                 nsnull);
  return result.forget();
}

PRBool
nsMediaQuery::Matches(nsPresContext* aPresContext,
                      nsMediaQueryResultCacheKey& aKey) const
{
  if (mHadUnknownExpression)
    return PR_FALSE;

  PRBool match =
    mMediaType == aPresContext->Medium() || mMediaType == nsGkAtoms::all;
  for (PRUint32 i = 0, i_end = mExpressions.Length(); match && i < i_end; ++i) {
    const nsMediaExpression &expr = mExpressions[i];
    nsCSSValue actual;
    nsresult rv = (expr.mFeature->mGetter)(aPresContext, actual);
    NS_ENSURE_SUCCESS(rv, PR_FALSE); // any better ideas?

    match = expr.Matches(aPresContext, actual);
    aKey.AddExpression(&expr, match);
  }

  return match == !mNegated;
}

NS_INTERFACE_MAP_BEGIN(nsMediaList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMediaList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(MediaList)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsMediaList)
NS_IMPL_RELEASE(nsMediaList)


nsMediaList::nsMediaList()
  : mIsEmpty(PR_TRUE)
  , mStyleSheet(nsnull)
{
}

nsMediaList::~nsMediaList()
{
}

nsresult
nsMediaList::GetText(nsAString& aMediaText)
{
  aMediaText.Truncate();

  if (mArray.Length() == 0 && !mIsEmpty) {
    aMediaText.AppendLiteral("not all");
  }

  for (PRInt32 i = 0, i_end = mArray.Length(); i < i_end; ++i) {
    nsMediaQuery* query = mArray[i];
    NS_ENSURE_TRUE(query, NS_ERROR_FAILURE);

    query->AppendToString(aMediaText);

    if (i + 1 < i_end) {
      aMediaText.AppendLiteral(", ");
    }
  }

  return NS_OK;
}

// XXXbz this is so ill-defined in the spec, it's not clear quite what
// it should be doing....
nsresult
nsMediaList::SetText(const nsAString& aMediaText)
{
  nsCOMPtr<nsICSSParser> parser;
  nsresult rv = NS_NewCSSParser(getter_AddRefs(parser));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool htmlMode = PR_FALSE;
  nsCOMPtr<nsIDOMStyleSheet> domSheet =
    do_QueryInterface(static_cast<nsICSSStyleSheet*>(mStyleSheet));
  if (domSheet) {
    nsCOMPtr<nsIDOMNode> node;
    domSheet->GetOwnerNode(getter_AddRefs(node));
    htmlMode = !!node;
  }

  return parser->ParseMediaList(nsString(aMediaText), nsnull, 0,
                                this, htmlMode);
}

PRBool
nsMediaList::Matches(nsPresContext* aPresContext,
                     nsMediaQueryResultCacheKey& aKey)
{
  for (PRInt32 i = 0, i_end = mArray.Length(); i < i_end; ++i) {
    if (mArray[i]->Matches(aPresContext, aKey)) {
      return PR_TRUE;
    }
  }
  return mIsEmpty;
}

nsresult
nsMediaList::SetStyleSheet(nsICSSStyleSheet *aSheet)
{
  NS_ASSERTION(aSheet == mStyleSheet || !aSheet || !mStyleSheet,
               "multiple style sheets competing for one media list");
  mStyleSheet = static_cast<nsCSSStyleSheet*>(aSheet);
  return NS_OK;
}

nsresult
nsMediaList::Clone(nsMediaList** aResult)
{
  nsRefPtr<nsMediaList> result = new nsMediaList();
  if (!result || !result->mArray.AppendElements(mArray.Length()))
    return NS_ERROR_OUT_OF_MEMORY;
  for (PRInt32 i = 0, i_end = mArray.Length(); i < i_end; ++i) {
    if (!(result->mArray[i] = mArray[i]->Clone())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  NS_ADDREF(*aResult = result);
  return NS_OK;
}

NS_IMETHODIMP
nsMediaList::GetMediaText(nsAString& aMediaText)
{
  return GetText(aMediaText);
}

// "sheet" should be an nsCSSStyleSheet and "doc" should be an
// nsCOMPtr<nsIDocument>
#define BEGIN_MEDIA_CHANGE(sheet, doc)                         \
  if (sheet) {                                                 \
    rv = sheet->GetOwningDocument(*getter_AddRefs(doc));       \
    NS_ENSURE_SUCCESS(rv, rv);                                 \
  }                                                            \
  mozAutoDocUpdate updateBatch(doc, UPDATE_STYLE, PR_TRUE);    \
  if (sheet) {                                                 \
    rv = sheet->WillDirty();                                   \
    NS_ENSURE_SUCCESS(rv, rv);                                 \
  }

#define END_MEDIA_CHANGE(sheet, doc)                           \
  if (sheet) {                                                 \
    sheet->DidDirty();                                         \
  }                                                            \
  /* XXXldb Pass something meaningful? */                      \
  if (doc) {                                                   \
    doc->StyleRuleChanged(sheet, nsnull, nsnull);              \
  }


NS_IMETHODIMP
nsMediaList::SetMediaText(const nsAString& aMediaText)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDocument> doc;

  BEGIN_MEDIA_CHANGE(mStyleSheet, doc)

  rv = SetText(aMediaText);
  if (NS_FAILED(rv))
    return rv;
  
  END_MEDIA_CHANGE(mStyleSheet, doc)

  return rv;
}
                               
NS_IMETHODIMP
nsMediaList::GetLength(PRUint32* aLength)
{
  NS_ENSURE_ARG_POINTER(aLength);

  *aLength = mArray.Length();
  return NS_OK;
}

NS_IMETHODIMP
nsMediaList::Item(PRUint32 aIndex, nsAString& aReturn)
{
  PRInt32 index = aIndex;
  if (0 <= index && index < Count()) {
    nsMediaQuery* query = mArray[index];
    NS_ENSURE_TRUE(query, NS_ERROR_FAILURE);

    aReturn.Truncate();
    query->AppendToString(aReturn);
  } else {
    SetDOMStringToNull(aReturn);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMediaList::DeleteMedium(const nsAString& aOldMedium)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDocument> doc;

  BEGIN_MEDIA_CHANGE(mStyleSheet, doc)
  
  rv = Delete(aOldMedium);
  if (NS_FAILED(rv))
    return rv;

  END_MEDIA_CHANGE(mStyleSheet, doc)
  
  return rv;
}

NS_IMETHODIMP
nsMediaList::AppendMedium(const nsAString& aNewMedium)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDocument> doc;

  BEGIN_MEDIA_CHANGE(mStyleSheet, doc)
  
  rv = Append(aNewMedium);
  if (NS_FAILED(rv))
    return rv;

  END_MEDIA_CHANGE(mStyleSheet, doc)
  
  return rv;
}

nsresult
nsMediaList::Delete(const nsAString& aOldMedium)
{
  if (aOldMedium.IsEmpty())
    return NS_ERROR_DOM_NOT_FOUND_ERR;

  for (PRInt32 i = 0, i_end = mArray.Length(); i < i_end; ++i) {
    nsMediaQuery* query = mArray[i];
    NS_ENSURE_TRUE(query, NS_ERROR_FAILURE);

    nsAutoString buf;
    query->AppendToString(buf);
    if (buf == aOldMedium) {
      mArray.RemoveElementAt(i);
      return NS_OK;
    }
  }

  return NS_ERROR_DOM_NOT_FOUND_ERR;
}

nsresult
nsMediaList::Append(const nsAString& aNewMedium)
{
  if (aNewMedium.IsEmpty())
    return NS_ERROR_DOM_NOT_FOUND_ERR;

  Delete(aNewMedium);

  nsresult rv = NS_OK;
  nsTArray<nsAutoPtr<nsMediaQuery> > buf;
#ifdef DEBUG
  PRBool ok = 
#endif
    mArray.SwapElements(buf);
  NS_ASSERTION(ok, "SwapElements should never fail when neither array "
                   "is an auto array");
  SetText(aNewMedium);
  if (mArray.Length() == 1) {
    nsMediaQuery *query = mArray[0].forget();
    if (!buf.AppendElement(query)) {
      delete query;
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
#ifdef DEBUG
  ok = 
#endif
    mArray.SwapElements(buf);
  NS_ASSERTION(ok, "SwapElements should never fail when neither array "
                   "is an auto array");
  return rv;
}

// -------------------------------
// CSS Style Sheet Inner Data Container
//


nsCSSStyleSheetInner::nsCSSStyleSheetInner(nsICSSStyleSheet* aPrimarySheet)
  : mSheets(),
    mComplete(PR_FALSE)
#ifdef DEBUG
    , mPrincipalSet(PR_FALSE)
#endif
{
  MOZ_COUNT_CTOR(nsCSSStyleSheetInner);
  mSheets.AppendElement(aPrimarySheet);

  mPrincipal = do_CreateInstance("@mozilla.org/nullprincipal;1");
}

static PRBool SetStyleSheetReference(nsICSSRule* aRule, void* aSheet)
{
  if (aRule) {
    aRule->SetStyleSheet((nsICSSStyleSheet*)aSheet);
  }
  return PR_TRUE;
}

static PRBool
CloneRuleInto(nsICSSRule* aRule, void* aArray)
{
  nsICSSRule* clone = nsnull;
  aRule->Clone(clone);
  if (clone) {
    static_cast<nsCOMArray<nsICSSRule>*>(aArray)->AppendObject(clone);
    NS_RELEASE(clone);
  }
  return PR_TRUE;
}

struct ChildSheetListBuilder {
  nsRefPtr<nsCSSStyleSheet>* sheetSlot;
  nsCSSStyleSheet* parent;

  void SetParentLinks(nsCSSStyleSheet* aSheet) {
    aSheet->mParent = parent;
    aSheet->SetOwningDocument(parent->mDocument);
  }
};
  
static PRBool
RebuildChildList(nsICSSRule* aRule, void* aBuilder)
{
  PRInt32 type;
  aRule->GetType(type);
  if (type == nsICSSRule::CHARSET_RULE) {
    return PR_TRUE;
  }

  if (type == nsICSSRule::NAMESPACE_RULE || type == nsICSSRule::MEDIA_RULE ||
      type == nsICSSRule::STYLE_RULE) {
    return PR_FALSE;
  }

  ChildSheetListBuilder* builder =
    static_cast<ChildSheetListBuilder*>(aBuilder);

  // XXXbz We really need to decomtaminate all this stuff.  Is there a reason
  // that I can't just QI to nsICSSImportRule and get an nsCSSStyleSheet
  // directly from it?
  nsCOMPtr<nsIDOMCSSImportRule> importRule(do_QueryInterface(aRule));
  NS_ASSERTION(importRule, "GetType lied");

  nsCOMPtr<nsIDOMCSSStyleSheet> childSheet;
  importRule->GetStyleSheet(getter_AddRefs(childSheet));

  // Have to do this QI to be safe, since XPConnect can fake
  // nsIDOMCSSStyleSheets
  nsCOMPtr<nsICSSStyleSheet> cssSheet = do_QueryInterface(childSheet);
  if (!cssSheet) {
    return PR_TRUE;
  }

  (*builder->sheetSlot) = static_cast<nsCSSStyleSheet*>(cssSheet.get());
  builder->SetParentLinks(*builder->sheetSlot);
  return PR_TRUE;
}

nsCSSStyleSheetInner::nsCSSStyleSheetInner(nsCSSStyleSheetInner& aCopy,
                                           nsCSSStyleSheet* aPrimarySheet)
  : mSheets(),
    mSheetURI(aCopy.mSheetURI),
    mOriginalSheetURI(aCopy.mOriginalSheetURI),
    mBaseURI(aCopy.mBaseURI),
    mPrincipal(aCopy.mPrincipal),
    mComplete(aCopy.mComplete)
#ifdef DEBUG
    , mPrincipalSet(aCopy.mPrincipalSet)
#endif
{
  MOZ_COUNT_CTOR(nsCSSStyleSheetInner);
  mSheets.AppendElement(aPrimarySheet);
  aCopy.mOrderedRules.EnumerateForwards(CloneRuleInto, &mOrderedRules);
  mOrderedRules.EnumerateForwards(SetStyleSheetReference, aPrimarySheet);

  ChildSheetListBuilder builder = { &mFirstChild, aPrimarySheet };
  mOrderedRules.EnumerateForwards(RebuildChildList, &builder);

  RebuildNameSpaces();
}

nsCSSStyleSheetInner::~nsCSSStyleSheetInner()
{
  MOZ_COUNT_DTOR(nsCSSStyleSheetInner);
  mOrderedRules.EnumerateForwards(SetStyleSheetReference, nsnull);
}

nsCSSStyleSheetInner* 
nsCSSStyleSheetInner::CloneFor(nsCSSStyleSheet* aPrimarySheet)
{
  return new nsCSSStyleSheetInner(*this, aPrimarySheet);
}

void
nsCSSStyleSheetInner::AddSheet(nsICSSStyleSheet* aSheet)
{
  mSheets.AppendElement(aSheet);
}

void
nsCSSStyleSheetInner::RemoveSheet(nsICSSStyleSheet* aSheet)
{
  if (1 == mSheets.Length()) {
    NS_ASSERTION(aSheet == mSheets.ElementAt(0), "bad parent");
    delete this;
    return;
  }
  if (aSheet == mSheets.ElementAt(0)) {
    mSheets.RemoveElementAt(0);
    NS_ASSERTION(mSheets.Length(), "no parents");
    mOrderedRules.EnumerateForwards(SetStyleSheetReference,
                                    mSheets.ElementAt(0));
  }
  else {
    mSheets.RemoveElement(aSheet);
  }
}

static void
AddNamespaceRuleToMap(nsICSSRule* aRule, nsXMLNameSpaceMap* aMap)
{
#ifdef DEBUG
  PRInt32 type;
  aRule->GetType(type);
  NS_ASSERTION(type == nsICSSRule::NAMESPACE_RULE, "Bogus rule type");
#endif

  nsCOMPtr<nsICSSNameSpaceRule> nameSpaceRule = do_QueryInterface(aRule);
  
  nsCOMPtr<nsIAtom> prefix;
  nsAutoString  urlSpec;
  nameSpaceRule->GetPrefix(*getter_AddRefs(prefix));
  nameSpaceRule->GetURLSpec(urlSpec);

  aMap->AddPrefix(prefix, urlSpec);
}

static PRBool
CreateNameSpace(nsICSSRule* aRule, void* aNameSpacePtr)
{
  PRInt32 type = nsICSSRule::UNKNOWN_RULE;
  aRule->GetType(type);
  if (nsICSSRule::NAMESPACE_RULE == type) {
    AddNamespaceRuleToMap(aRule,
                          static_cast<nsXMLNameSpaceMap*>(aNameSpacePtr));
    return PR_TRUE;
  }
  // stop if not namespace, import or charset because namespace can't follow
  // anything else
  return (nsICSSRule::CHARSET_RULE == type || nsICSSRule::IMPORT_RULE == type);
}

void 
nsCSSStyleSheetInner::RebuildNameSpaces()
{
  // Just nuke our existing namespace map, if any
  if (NS_SUCCEEDED(CreateNamespaceMap())) {
    mOrderedRules.EnumerateForwards(CreateNameSpace, mNameSpaceMap);
  }
}

nsresult
nsCSSStyleSheetInner::CreateNamespaceMap()
{
  mNameSpaceMap = nsXMLNameSpaceMap::Create();
  NS_ENSURE_TRUE(mNameSpaceMap, NS_ERROR_OUT_OF_MEMORY);
  // Override the default namespace map behavior for the null prefix to
  // return the wildcard namespace instead of the null namespace.
  mNameSpaceMap->AddPrefix(nsnull, kNameSpaceID_Unknown);
  return NS_OK;
}

// -------------------------------
// CSS Style Sheet
//

nsCSSStyleSheet::nsCSSStyleSheet()
  : nsICSSStyleSheet(),
    mRefCnt(0),
    mTitle(), 
    mMedia(nsnull),
    mParent(nsnull),
    mOwnerRule(nsnull),
    mRuleCollection(nsnull),
    mDocument(nsnull),
    mOwningNode(nsnull),
    mDisabled(PR_FALSE),
    mDirty(PR_FALSE),
    mRuleProcessors(nsnull)
{

  mInner = new nsCSSStyleSheetInner(this);
}

nsCSSStyleSheet::nsCSSStyleSheet(const nsCSSStyleSheet& aCopy,
                                 nsICSSStyleSheet* aParentToUse,
                                 nsICSSImportRule* aOwnerRuleToUse,
                                 nsIDocument* aDocumentToUse,
                                 nsIDOMNode* aOwningNodeToUse)
  : nsICSSStyleSheet(),
    mRefCnt(0),
    mTitle(aCopy.mTitle), 
    mMedia(nsnull),
    mParent(aParentToUse),
    mOwnerRule(aOwnerRuleToUse),
    mRuleCollection(nsnull), // re-created lazily
    mDocument(aDocumentToUse),
    mOwningNode(aOwningNodeToUse),
    mDisabled(aCopy.mDisabled),
    mDirty(PR_FALSE),
    mInner(aCopy.mInner),
    mRuleProcessors(nsnull)
{

  mInner->AddSheet(this);

  if (aCopy.mRuleCollection && 
      aCopy.mRuleCollection->mRulesAccessed) {  // CSSOM's been there, force full copy now
    NS_ASSERTION(mInner->mComplete, "Why have rules been accessed on an incomplete sheet?");
    EnsureUniqueInner();
  }

  if (aCopy.mMedia) {
    // XXX This is wrong; we should be keeping @import rules and
    // sheets in sync!
    aCopy.mMedia->Clone(getter_AddRefs(mMedia));
  }
}

nsCSSStyleSheet::~nsCSSStyleSheet()
{
  for (nsCSSStyleSheet* child = mInner->mFirstChild;
       child;
       child = child->mNext) {
    // XXXbz this is a little bogus; see the XXX comment where we
    // declare mFirstChild.
    if (child->mParent == this) {
      child->mParent = nsnull;
      child->mDocument = nsnull;
    }
  }
  if (nsnull != mRuleCollection) {
    mRuleCollection->DropReference();
    NS_RELEASE(mRuleCollection);
  }
  if (mMedia) {
    mMedia->SetStyleSheet(nsnull);
    mMedia = nsnull;
  }
  mInner->RemoveSheet(this);
  // XXX The document reference is not reference counted and should
  // not be released. The document will let us know when it is going
  // away.
  if (mRuleProcessors) {
    NS_ASSERTION(mRuleProcessors->Length() == 0, "destructing sheet with rule processor reference");
    delete mRuleProcessors; // weak refs, should be empty here anyway
  }
}


// QueryInterface implementation for nsCSSStyleSheet
NS_INTERFACE_MAP_BEGIN(nsCSSStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsICSSStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsIStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICSSStyleSheet)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSStyleSheet)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsCSSStyleSheet)
NS_IMPL_RELEASE(nsCSSStyleSheet)


NS_IMETHODIMP
nsCSSStyleSheet::AddRuleProcessor(nsCSSRuleProcessor* aProcessor)
{
  if (! mRuleProcessors) {
    mRuleProcessors = new nsAutoTArray<nsCSSRuleProcessor*, 8>();
    if (!mRuleProcessors)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ASSERTION(-1 == mRuleProcessors->IndexOf(aProcessor),
               "processor already registered");
  mRuleProcessors->AppendElement(aProcessor); // weak ref
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::DropRuleProcessor(nsCSSRuleProcessor* aProcessor)
{
  if (!mRuleProcessors)
    return NS_ERROR_FAILURE;
  return mRuleProcessors->RemoveElement(aProcessor)
           ? NS_OK
           : NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsCSSStyleSheet::SetURIs(nsIURI* aSheetURI, nsIURI* aOriginalSheetURI,
                         nsIURI* aBaseURI)
{
  NS_PRECONDITION(aSheetURI && aBaseURI, "null ptr");

  NS_ASSERTION(mInner->mOrderedRules.Count() == 0 && !mInner->mComplete,
               "Can't call SetURL on sheets that are complete or have rules");

  mInner->mSheetURI = aSheetURI;
  mInner->mOriginalSheetURI = aOriginalSheetURI;
  mInner->mBaseURI = aBaseURI;
  return NS_OK;
}

void
nsCSSStyleSheet::SetPrincipal(nsIPrincipal* aPrincipal)
{
  NS_PRECONDITION(!mInner->mPrincipalSet,
                  "Should have an inner whose principal has not yet been set");
  if (aPrincipal) {
    mInner->mPrincipal = aPrincipal;
#ifdef DEBUG
    mInner->mPrincipalSet = PR_TRUE;
#endif
  }
}

nsIPrincipal*
nsCSSStyleSheet::Principal() const
{
  return mInner->mPrincipal;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetSheetURI(nsIURI** aSheetURI) const
{
  NS_IF_ADDREF(*aSheetURI = mInner->mSheetURI.get());
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetBaseURI(nsIURI** aBaseURI) const
{
  NS_IF_ADDREF(*aBaseURI = mInner->mBaseURI.get());
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::SetTitle(const nsAString& aTitle)
{
  mTitle = aTitle;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetType(nsString& aType) const
{
  aType.AssignLiteral("text/css");
  return NS_OK;
}

PRBool
nsCSSStyleSheet::UseForPresentation(nsPresContext* aPresContext,
                                    nsMediaQueryResultCacheKey& aKey) const
{
  if (mMedia) {
    return mMedia->Matches(aPresContext, aKey);
  }
  return PR_TRUE;
}


NS_IMETHODIMP
nsCSSStyleSheet::SetMedia(nsMediaList* aMedia)
{
  mMedia = aMedia;
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsCSSStyleSheet::HasRules() const
{
  PRInt32 count;
  StyleRuleCount(count);
  return count != 0;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetApplicable(PRBool& aApplicable) const
{
  aApplicable = !mDisabled && mInner->mComplete;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::SetEnabled(PRBool aEnabled)
{
  // Internal method, so callers must handle BeginUpdate/EndUpdate
  PRBool oldDisabled = mDisabled;
  mDisabled = !aEnabled;

  if (mInner->mComplete && oldDisabled != mDisabled) {
    ClearRuleCascades();

    if (mDocument) {
      mDocument->SetStyleSheetApplicableState(this, !mDisabled);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetComplete(PRBool& aComplete) const
{
  aComplete = mInner->mComplete;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::SetComplete()
{
  NS_ASSERTION(!mDirty, "Can't set a dirty sheet complete!");
  mInner->mComplete = PR_TRUE;
  if (mDocument && !mDisabled) {
    // Let the document know
    mDocument->BeginUpdate(UPDATE_STYLE);
    mDocument->SetStyleSheetApplicableState(this, PR_TRUE);
    mDocument->EndUpdate(UPDATE_STYLE);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetParentSheet(nsIStyleSheet*& aParent) const
{
  aParent = mParent;
  NS_IF_ADDREF(aParent);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetOwningDocument(nsIDocument*& aDocument) const
{
  aDocument = mDocument;
  NS_IF_ADDREF(aDocument);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::SetOwningDocument(nsIDocument* aDocument)
{ // not ref counted
  mDocument = aDocument;
  // Now set the same document on all our child sheets....
  // XXXbz this is a little bogus; see the XXX comment where we
  // declare mFirstChild.
  for (nsCSSStyleSheet* child = mInner->mFirstChild;
       child; child = child->mNext) {
    if (child->mParent == this) {
      child->SetOwningDocument(aDocument);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::SetOwningNode(nsIDOMNode* aOwningNode)
{ // not ref counted
  mOwningNode = aOwningNode;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::SetOwnerRule(nsICSSImportRule* aOwnerRule)
{ // not ref counted
  mOwnerRule = aOwnerRule;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetOwnerRule(nsICSSImportRule** aOwnerRule)
{
  *aOwnerRule = mOwnerRule;
  NS_IF_ADDREF(*aOwnerRule);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::AppendStyleSheet(nsICSSStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");

  if (NS_SUCCEEDED(WillDirty())) {
    nsCSSStyleSheet* sheet = (nsCSSStyleSheet*)aSheet;

    nsRefPtr<nsCSSStyleSheet>* tail = &mInner->mFirstChild;
    while (*tail) {
      tail = &(*tail)->mNext;
    }
    *tail = sheet;
  
    // This is not reference counted. Our parent tells us when
    // it's going away.
    sheet->mParent = this;
    sheet->mDocument = mDocument;
    DidDirty();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::InsertStyleSheetAt(nsICSSStyleSheet* aSheet, PRInt32 aIndex)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");

  nsresult result = WillDirty();

  if (NS_SUCCEEDED(result)) {
    nsCSSStyleSheet* sheet = (nsCSSStyleSheet*)aSheet;

    nsRefPtr<nsCSSStyleSheet>* tail = &mInner->mFirstChild;
    while (*tail && aIndex) {
      --aIndex;
      tail = &(*tail)->mNext;
    }
    sheet->mNext = *tail;
    *tail = sheet;

    // This is not reference counted. Our parent tells us when
    // it's going away.
    sheet->mParent = this;
    sheet->mDocument = mDocument;
    DidDirty();
  }
  return result;
}

NS_IMETHODIMP
nsCSSStyleSheet::PrependStyleRule(nsICSSRule* aRule)
{
  NS_PRECONDITION(nsnull != aRule, "null arg");

  if (NS_SUCCEEDED(WillDirty())) {
    mInner->mOrderedRules.InsertObjectAt(aRule, 0);
    aRule->SetStyleSheet(this);
    DidDirty();

    PRInt32 type = nsICSSRule::UNKNOWN_RULE;
    aRule->GetType(type);
    if (nsICSSRule::NAMESPACE_RULE == type) {
      // no api to prepend a namespace (ugh), release old ones and re-create them all
      mInner->RebuildNameSpaces();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::AppendStyleRule(nsICSSRule* aRule)
{
  NS_PRECONDITION(nsnull != aRule, "null arg");

  if (NS_SUCCEEDED(WillDirty())) {
    mInner->mOrderedRules.AppendObject(aRule);
    aRule->SetStyleSheet(this);
    DidDirty();

    PRInt32 type = nsICSSRule::UNKNOWN_RULE;
    aRule->GetType(type);
    if (nsICSSRule::NAMESPACE_RULE == type) {
      nsresult rv = RegisterNamespaceRule(aRule);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::ReplaceStyleRule(nsICSSRule* aOld, nsICSSRule* aNew)
{
  NS_PRECONDITION(mInner->mOrderedRules.Count() != 0, "can't have old rule");
  NS_PRECONDITION(mInner->mComplete, "No replacing in an incomplete sheet!");

  if (NS_SUCCEEDED(WillDirty())) {
    PRInt32 index = mInner->mOrderedRules.IndexOf(aOld);
    NS_ENSURE_TRUE(index != -1, NS_ERROR_UNEXPECTED);
    mInner->mOrderedRules.ReplaceObjectAt(aNew, index);

    aNew->SetStyleSheet(this);
    aOld->SetStyleSheet(nsnull);
    DidDirty();
#ifdef DEBUG
    PRInt32 type = nsICSSRule::UNKNOWN_RULE;
    aNew->GetType(type);
    NS_ASSERTION(nsICSSRule::NAMESPACE_RULE != type, "not yet implemented");
    aOld->GetType(type);
    NS_ASSERTION(nsICSSRule::NAMESPACE_RULE != type, "not yet implemented");
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::StyleRuleCount(PRInt32& aCount) const
{
  aCount = mInner->mOrderedRules.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const
{
  // Important: If this function is ever made scriptable, we must add
  // a security check here. See GetCssRules below for an example.
  aRule = mInner->mOrderedRules.SafeObjectAt(aIndex);
  if (aRule) {
    NS_ADDREF(aRule);
    return NS_OK;
  }

  return NS_ERROR_ILLEGAL_VALUE;
}

nsXMLNameSpaceMap*
nsCSSStyleSheet::GetNameSpaceMap() const
{
  return mInner->mNameSpaceMap;
}

NS_IMETHODIMP
nsCSSStyleSheet::StyleSheetCount(PRInt32& aCount) const
{
  // XXX Far from an ideal way to do this, but the hope is that
  // it won't be done too often. If it is, we might want to 
  // consider storing the children in an array.
  aCount = 0;

  const nsCSSStyleSheet* child = mInner->mFirstChild;
  while (child) {
    aCount++;
    child = child->mNext;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetStyleSheetAt(PRInt32 aIndex, nsICSSStyleSheet*& aSheet) const
{
  // XXX Ughh...an O(n^2) method for doing iteration. Again, we hope
  // that this isn't done too often. If it is, we need to change the
  // underlying storage mechanism
  aSheet = nsnull;

  nsCSSStyleSheet* child = mInner->mFirstChild;
  while (child && (0 != aIndex)) {
    --aIndex;
    child = child->mNext;
  }
    
  NS_IF_ADDREF(aSheet = child);

  return NS_OK;
}

nsresult  
nsCSSStyleSheet::EnsureUniqueInner()
{
  if (1 < mInner->mSheets.Length()) {
    nsCSSStyleSheetInner* clone = mInner->CloneFor(this);
    if (clone) {
      mInner->RemoveSheet(this);
      mInner = clone;
    }
    else {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::Clone(nsICSSStyleSheet* aCloneParent,
                       nsICSSImportRule* aCloneOwnerRule,
                       nsIDocument* aCloneDocument,
                       nsIDOMNode* aCloneOwningNode,
                       nsICSSStyleSheet** aClone) const
{
  NS_PRECONDITION(aClone, "Null out param!");
  nsCSSStyleSheet* clone = new nsCSSStyleSheet(*this,
                                               aCloneParent,
                                               aCloneOwnerRule,
                                               aCloneDocument,
                                               aCloneOwningNode);
  if (clone) {
    *aClone = static_cast<nsICSSStyleSheet*>(clone);
    NS_ADDREF(*aClone);
  }
  return NS_OK;
}

#ifdef DEBUG
static void
ListRules(const nsCOMArray<nsICSSRule>& aRules, FILE* aOut, PRInt32 aIndent)
{
  for (PRInt32 index = aRules.Count() - 1; index >= 0; --index) {
    aRules.ObjectAt(index)->List(aOut, aIndent);
  }
}

struct ListEnumData {
  ListEnumData(FILE* aOut, PRInt32 aIndent)
    : mOut(aOut),
      mIndent(aIndent)
  {
  }
  FILE*   mOut;
  PRInt32 mIndent;
};

void nsCSSStyleSheet::List(FILE* out, PRInt32 aIndent) const
{

  PRInt32 index;

  // Indent
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("CSS Style Sheet: ", out);
  nsCAutoString urlSpec;
  nsresult rv = mInner->mSheetURI->GetSpec(urlSpec);
  if (NS_SUCCEEDED(rv) && !urlSpec.IsEmpty()) {
    fputs(urlSpec.get(), out);
  }

  if (mMedia) {
    fputs(" media: ", out);
    nsAutoString  buffer;
    mMedia->GetText(buffer);
    fputs(NS_ConvertUTF16toUTF8(buffer).get(), out);
  }
  fputs("\n", out);

  for (const nsCSSStyleSheet*  child = mInner->mFirstChild;
       child;
       child = child->mNext) {
    child->List(out, aIndent + 1);
  }

  fputs("Rules in source order:\n", out);
  ListRules(mInner->mOrderedRules, out, aIndent);
}
#endif

void 
nsCSSStyleSheet::ClearRuleCascades()
{
  if (mRuleProcessors) {
    nsCSSRuleProcessor **iter = mRuleProcessors->Elements(),
                       **end = iter + mRuleProcessors->Length();
    for(; iter != end; ++iter) {
      (*iter)->ClearRuleCascades();
    }
  }
  if (mParent) {
    nsCSSStyleSheet* parent = (nsCSSStyleSheet*)mParent;
    parent->ClearRuleCascades();
  }
}

nsresult
nsCSSStyleSheet::WillDirty()
{
  if (!mInner->mComplete) {
    // Do nothing
    return NS_OK;
  }
  
  return EnsureUniqueInner();
}

void
nsCSSStyleSheet::DidDirty()
{
  ClearRuleCascades();
  mDirty = PR_TRUE;
}

nsresult
nsCSSStyleSheet::SubjectSubsumesInnerPrincipal() const
{
  // Get the security manager and do the subsumes check
  nsIScriptSecurityManager *securityManager =
    nsContentUtils::GetSecurityManager();

  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  securityManager->GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));

  if (!subjectPrincipal) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  PRBool subsumes;
  nsresult rv = subjectPrincipal->Subsumes(mInner->mPrincipal, &subsumes);
  NS_ENSURE_SUCCESS(rv, rv);

  if (subsumes) {
    return NS_OK;
  }
  
  if (!nsContentUtils::IsCallerTrustedForWrite()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return NS_OK;
}

nsresult
nsCSSStyleSheet::RegisterNamespaceRule(nsICSSRule* aRule)
{
  if (!mInner->mNameSpaceMap) {
    nsresult rv = mInner->CreateNamespaceMap();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  AddNamespaceRuleToMap(aRule, mInner->mNameSpaceMap);
  return NS_OK;
}

NS_IMETHODIMP 
nsCSSStyleSheet::IsModified(PRBool* aSheetModified) const
{
  *aSheetModified = mDirty;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::SetModified(PRBool aModified)
{
  mDirty = aModified;
  return NS_OK;
}

  // nsIDOMStyleSheet interface
NS_IMETHODIMP    
nsCSSStyleSheet::GetType(nsAString& aType)
{
  aType.AssignLiteral("text/css");
  return NS_OK;
}

NS_IMETHODIMP    
nsCSSStyleSheet::GetDisabled(PRBool* aDisabled)
{
  *aDisabled = mDisabled;
  return NS_OK;
}

NS_IMETHODIMP    
nsCSSStyleSheet::SetDisabled(PRBool aDisabled)
{
  // DOM method, so handle BeginUpdate/EndUpdate
  MOZ_AUTO_DOC_UPDATE(mDocument, UPDATE_STYLE, PR_TRUE);
  nsresult rv = nsCSSStyleSheet::SetEnabled(!aDisabled);
  return rv;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetOwnerNode(nsIDOMNode** aOwnerNode)
{
  *aOwnerNode = mOwningNode;
  NS_IF_ADDREF(*aOwnerNode);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetParentStyleSheet(nsIDOMStyleSheet** aParentStyleSheet)
{
  NS_ENSURE_ARG_POINTER(aParentStyleSheet);

  nsresult rv = NS_OK;

  if (mParent) {
    rv =  mParent->QueryInterface(NS_GET_IID(nsIDOMStyleSheet),
                                  (void **)aParentStyleSheet);
  } else {
    *aParentStyleSheet = nsnull;
  }

  return rv;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetHref(nsAString& aHref)
{
  if (mInner->mOriginalSheetURI) {
    nsCAutoString str;
    mInner->mOriginalSheetURI->GetSpec(str);
    CopyUTF8toUTF16(str, aHref);
  } else {
    SetDOMStringToNull(aHref);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetTitle(nsString& aTitle) const
{
  aTitle = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetTitle(nsAString& aTitle)
{
  aTitle.Assign(mTitle);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetMedia(nsIDOMMediaList** aMedia)
{
  NS_ENSURE_ARG_POINTER(aMedia);
  *aMedia = nsnull;

  if (!mMedia) {
    mMedia = new nsMediaList();
    NS_ENSURE_TRUE(mMedia, NS_ERROR_OUT_OF_MEMORY);
    mMedia->SetStyleSheet(this);
  }

  *aMedia = mMedia;
  NS_ADDREF(*aMedia);

  return NS_OK;
}

NS_IMETHODIMP    
nsCSSStyleSheet::GetOwnerRule(nsIDOMCSSRule** aOwnerRule)
{
  if (mOwnerRule) {
    return mOwnerRule->GetDOMRule(aOwnerRule);
  }

  *aOwnerRule = nsnull;
  return NS_OK;    
}

NS_IMETHODIMP    
nsCSSStyleSheet::GetCssRules(nsIDOMCSSRuleList** aCssRules)
{
  // No doing this on incomplete sheets!
  PRBool complete;
  GetComplete(complete);
  if (!complete) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }
  
  //-- Security check: Only scripts whose principal subsumes that of the
  //   style sheet can access rule collections.
  nsresult rv = SubjectSubsumesInnerPrincipal();
  NS_ENSURE_SUCCESS(rv, rv);

  // OK, security check passed, so get the rule collection
  if (nsnull == mRuleCollection) {
    mRuleCollection = new CSSRuleListImpl(this);
    if (nsnull == mRuleCollection) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mRuleCollection);
  }

  *aCssRules = mRuleCollection;
  NS_ADDREF(mRuleCollection);

  return NS_OK;
}

NS_IMETHODIMP    
nsCSSStyleSheet::InsertRule(const nsAString& aRule, 
                            PRUint32 aIndex, 
                            PRUint32* aReturn)
{
  //-- Security check: Only scripts whose principal subsumes that of the
  //   style sheet can modify rule collections.
  nsresult rv = SubjectSubsumesInnerPrincipal();
  NS_ENSURE_SUCCESS(rv, rv);

  return InsertRuleInternal(aRule, aIndex, aReturn);
}

NS_IMETHODIMP
nsCSSStyleSheet::InsertRuleInternal(const nsAString& aRule, 
                                    PRUint32 aIndex, 
                                    PRUint32* aReturn)
{
  // No doing this if the sheet is not complete!
  PRBool complete;
  GetComplete(complete);
  if (!complete) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  if (aRule.IsEmpty()) {
    // Nothing to do here
    return NS_OK;
  }
  
  nsresult result;
  result = WillDirty();
  if (NS_FAILED(result))
    return result;
  
  if (aIndex > PRUint32(mInner->mOrderedRules.Count()))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  
  NS_ASSERTION(PRUint32(mInner->mOrderedRules.Count()) <= PR_INT32_MAX,
               "Too many style rules!");

  // Hold strong ref to the CSSLoader in case the document update
  // kills the document
  nsCOMPtr<nsICSSLoader> loader;
  if (mDocument) {
    loader = mDocument->CSSLoader();
    NS_ASSERTION(loader, "Document with no CSS loader!");
  }
  
  nsCOMPtr<nsICSSParser> css;
  if (loader) {
    result = loader->GetParserFor(this, getter_AddRefs(css));
  }
  else {
    result = NS_NewCSSParser(getter_AddRefs(css));
    if (css) {
      css->SetStyleSheet(this);
    }
  }
  if (NS_FAILED(result))
    return result;

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, PR_TRUE);

  nsCOMArray<nsICSSRule> rules;
  result = css->ParseRule(aRule, mInner->mSheetURI, mInner->mBaseURI,
                          mInner->mPrincipal, rules);
  if (NS_FAILED(result))
    return result;
  
  PRInt32 rulecount = rules.Count();
  if (rulecount == 0) {
    // Since we know aRule was not an empty string, just throw
    return NS_ERROR_DOM_SYNTAX_ERR;
  }
  
  // Hierarchy checking.  Just check the first and last rule in the list.
  
  // check that we're not inserting before a charset rule
  PRInt32 nextType = nsICSSRule::UNKNOWN_RULE;
  nsICSSRule* nextRule = mInner->mOrderedRules.SafeObjectAt(aIndex);
  if (nextRule) {
    nextRule->GetType(nextType);
    if (nextType == nsICSSRule::CHARSET_RULE) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }

    // check last rule in list
    nsICSSRule* lastRule = rules.ObjectAt(rulecount - 1);
    PRInt32 lastType = nsICSSRule::UNKNOWN_RULE;
    lastRule->GetType(lastType);
    
    if (nextType == nsICSSRule::IMPORT_RULE &&
        lastType != nsICSSRule::CHARSET_RULE &&
        lastType != nsICSSRule::IMPORT_RULE) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }
    
    if (nextType == nsICSSRule::NAMESPACE_RULE &&
        lastType != nsICSSRule::CHARSET_RULE &&
        lastType != nsICSSRule::IMPORT_RULE &&
        lastType != nsICSSRule::NAMESPACE_RULE) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    } 
  }
  
  // check first rule in list
  nsICSSRule* firstRule = rules.ObjectAt(0);
  PRInt32 firstType = nsICSSRule::UNKNOWN_RULE;
  firstRule->GetType(firstType);
  if (aIndex != 0) {
    if (firstType == nsICSSRule::CHARSET_RULE) { // no inserting charset at nonzero position
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }
  
    nsICSSRule* prevRule = mInner->mOrderedRules.SafeObjectAt(aIndex - 1);
    PRInt32 prevType = nsICSSRule::UNKNOWN_RULE;
    prevRule->GetType(prevType);

    if (firstType == nsICSSRule::IMPORT_RULE &&
        prevType != nsICSSRule::CHARSET_RULE &&
        prevType != nsICSSRule::IMPORT_RULE) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }

    if (firstType == nsICSSRule::NAMESPACE_RULE &&
        prevType != nsICSSRule::CHARSET_RULE &&
        prevType != nsICSSRule::IMPORT_RULE &&
        prevType != nsICSSRule::NAMESPACE_RULE) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }
  }
  
  PRBool insertResult = mInner->mOrderedRules.InsertObjectsAt(rules, aIndex);
  NS_ENSURE_TRUE(insertResult, NS_ERROR_OUT_OF_MEMORY);
  DidDirty();

  for (PRInt32 counter = 0; counter < rulecount; counter++) {
    nsICSSRule* cssRule = rules.ObjectAt(counter);
    cssRule->SetStyleSheet(this);
    
    PRInt32 type = nsICSSRule::UNKNOWN_RULE;
    cssRule->GetType(type);
    if (type == nsICSSRule::NAMESPACE_RULE) {
      // XXXbz does this screw up when inserting a namespace rule before
      // another namespace rule that binds the same prefix to a different
      // namespace?
      result = RegisterNamespaceRule(cssRule);
      NS_ENSURE_SUCCESS(result, result);
    }

    // We don't notify immediately for @import rules, but rather when
    // the sheet the rule is importing is loaded
    PRBool notify = PR_TRUE;
    if (type == nsICSSRule::IMPORT_RULE) {
      nsCOMPtr<nsIDOMCSSImportRule> importRule(do_QueryInterface(cssRule));
      NS_ASSERTION(importRule, "Rule which has type IMPORT_RULE and does not implement nsIDOMCSSImportRule!");
      nsCOMPtr<nsIDOMCSSStyleSheet> childSheet;
      importRule->GetStyleSheet(getter_AddRefs(childSheet));
      if (!childSheet) {
        notify = PR_FALSE;
      }
    }
    if (mDocument && notify) {
      mDocument->StyleRuleAdded(this, cssRule);
    }
  }
  
  if (loader) {
    loader->RecycleParser(css);
  }
  
  *aReturn = aIndex;
  return NS_OK;
}

NS_IMETHODIMP    
nsCSSStyleSheet::DeleteRule(PRUint32 aIndex)
{
  nsresult result = NS_ERROR_DOM_INDEX_SIZE_ERR;
  // No doing this if the sheet is not complete!
  PRBool complete;
  GetComplete(complete);
  if (!complete) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  //-- Security check: Only scripts whose principal subsumes that of the
  //   style sheet can modify rule collections.
  nsresult rv = SubjectSubsumesInnerPrincipal();
  NS_ENSURE_SUCCESS(rv, rv);

  // XXX TBI: handle @rule types
  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, PR_TRUE);
    
  result = WillDirty();

  if (NS_SUCCEEDED(result)) {
    if (aIndex >= PRUint32(mInner->mOrderedRules.Count()))
      return NS_ERROR_DOM_INDEX_SIZE_ERR;

    NS_ASSERTION(PRUint32(mInner->mOrderedRules.Count()) <= PR_INT32_MAX,
                 "Too many style rules!");

    // Hold a strong ref to the rule so it doesn't die when we RemoveObjectAt
    nsCOMPtr<nsICSSRule> rule = mInner->mOrderedRules.ObjectAt(aIndex);
    if (rule) {
      mInner->mOrderedRules.RemoveObjectAt(aIndex);
      rule->SetStyleSheet(nsnull);
      DidDirty();

      if (mDocument) {
        mDocument->StyleRuleRemoved(this, rule);
      }
    }
  }

  return result;
}

NS_IMETHODIMP
nsCSSStyleSheet::DeleteRuleFromGroup(nsICSSGroupRule* aGroup, PRUint32 aIndex)
{
  NS_ENSURE_ARG_POINTER(aGroup);
  NS_ASSERTION(mInner->mComplete, "No deleting from an incomplete sheet!");
  nsresult result;
  nsCOMPtr<nsICSSRule> rule;
  result = aGroup->GetStyleRuleAt(aIndex, *getter_AddRefs(rule));
  NS_ENSURE_SUCCESS(result, result);
  
  // check that the rule actually belongs to this sheet!
  nsCOMPtr<nsIStyleSheet> ruleSheet;
  rule->GetStyleSheet(*getter_AddRefs(ruleSheet));
  if (this != ruleSheet) {
    return NS_ERROR_INVALID_ARG;
  }

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, PR_TRUE);
  
  result = WillDirty();
  NS_ENSURE_SUCCESS(result, result);

  result = aGroup->DeleteStyleRuleAt(aIndex);
  NS_ENSURE_SUCCESS(result, result);
  
  rule->SetStyleSheet(nsnull);
  
  DidDirty();

  if (mDocument) {
    mDocument->StyleRuleRemoved(this, rule);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::InsertRuleIntoGroup(const nsAString & aRule,
                                     nsICSSGroupRule* aGroup,
                                     PRUint32 aIndex,
                                     PRUint32* _retval)
{
  nsresult result;
  NS_ASSERTION(mInner->mComplete, "No inserting into an incomplete sheet!");
  // check that the group actually belongs to this sheet!
  nsCOMPtr<nsIStyleSheet> groupSheet;
  aGroup->GetStyleSheet(*getter_AddRefs(groupSheet));
  if (this != groupSheet) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aRule.IsEmpty()) {
    // Nothing to do here
    return NS_OK;
  }
  
  // Hold strong ref to the CSSLoader in case the document update
  // kills the document
  nsCOMPtr<nsICSSLoader> loader;
  if (mDocument) {
    loader = mDocument->CSSLoader();
    NS_ASSERTION(loader, "Document with no CSS loader!");
  }

  nsCOMPtr<nsICSSParser> css;
  if (loader) {
    result = loader->GetParserFor(this, getter_AddRefs(css));
  }
  else {
    result = NS_NewCSSParser(getter_AddRefs(css));
    if (css) {
      css->SetStyleSheet(this);
    }
  }
  NS_ENSURE_SUCCESS(result, result);

  // parse and grab the rule
  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, PR_TRUE);

  result = WillDirty();
  NS_ENSURE_SUCCESS(result, result);

  nsCOMArray<nsICSSRule> rules;
  result = css->ParseRule(aRule, mInner->mSheetURI, mInner->mBaseURI,
                          mInner->mPrincipal, rules);
  NS_ENSURE_SUCCESS(result, result);

  PRInt32 rulecount = rules.Count();
  if (rulecount == 0) {
    // Since we know aRule was not an empty string, just throw
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  PRInt32 counter;
  nsICSSRule* rule;
  for (counter = 0; counter < rulecount; counter++) {
    // Only rulesets are allowed in a group as of CSS2
    PRInt32 type = nsICSSRule::UNKNOWN_RULE;
    rule = rules.ObjectAt(counter);
    rule->GetType(type);
    if (type != nsICSSRule::STYLE_RULE) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }
  }
  
  result = aGroup->InsertStyleRulesAt(aIndex, rules);
  NS_ENSURE_SUCCESS(result, result);
  DidDirty();
  for (counter = 0; counter < rulecount; counter++) {
    rule = rules.ObjectAt(counter);
  
    if (mDocument) {
      mDocument->StyleRuleAdded(this, rule);
    }
  }

  if (loader) {
    loader->RecycleParser(css);
  }

  *_retval = aIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::ReplaceRuleInGroup(nsICSSGroupRule* aGroup,
                                      nsICSSRule* aOld, nsICSSRule* aNew)
{
  nsresult result;
  NS_PRECONDITION(mInner->mComplete, "No replacing in an incomplete sheet!");
#ifdef DEBUG
  {
    nsCOMPtr<nsIStyleSheet> groupSheet;
    aGroup->GetStyleSheet(*getter_AddRefs(groupSheet));
    NS_ASSERTION(this == groupSheet, "group doesn't belong to this sheet");
  }
#endif
  result = WillDirty();
  NS_ENSURE_SUCCESS(result, result);

  result = aGroup->ReplaceStyleRule(aOld, aNew);
  DidDirty();
  return result;
}

// nsICSSLoaderObserver implementation
NS_IMETHODIMP
nsCSSStyleSheet::StyleSheetLoaded(nsICSSStyleSheet* aSheet,
                                  PRBool aWasAlternate,
                                  nsresult aStatus)
{
#ifdef DEBUG
  nsCOMPtr<nsIStyleSheet> styleSheet(do_QueryInterface(aSheet));
  NS_ASSERTION(styleSheet, "Sheet not implementing nsIStyleSheet!\n");
  nsCOMPtr<nsIStyleSheet> parentSheet;
  aSheet->GetParentSheet(*getter_AddRefs(parentSheet));
  nsCOMPtr<nsIStyleSheet> thisSheet;
  QueryInterface(NS_GET_IID(nsIStyleSheet), getter_AddRefs(thisSheet));
  NS_ASSERTION(thisSheet == parentSheet, "We are being notified of a sheet load for a sheet that is not our child!\n");
#endif
  
  if (mDocument && NS_SUCCEEDED(aStatus)) {
    nsCOMPtr<nsICSSImportRule> ownerRule;
    aSheet->GetOwnerRule(getter_AddRefs(ownerRule));
    
    mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, PR_TRUE);

    // XXXldb @import rules shouldn't even implement nsIStyleRule (but
    // they do)!
    nsCOMPtr<nsIStyleRule> styleRule(do_QueryInterface(ownerRule));
    
    mDocument->StyleRuleAdded(this, styleRule);
  }

  return NS_OK;
}

nsresult
NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult)
{
  *aInstancePtrResult = nsnull;
  nsCSSStyleSheet  *it = new nsCSSStyleSheet();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(it);

  if (!it->mInner || !it->mInner->mPrincipal) {
    NS_RELEASE(it);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  *aInstancePtrResult = it;
  return NS_OK;
}
