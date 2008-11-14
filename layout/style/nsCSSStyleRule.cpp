/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   David Hyatt <hyatt@netscape.com>
 *   Daniel Glazman <glazman@netscape.com>
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
 * representation of CSS style rules (selectors+declaration), CSS
 * selectors, and DOM objects for style rules, selectors, and
 * declarations
 */

#include "nsCOMPtr.h"
#include "nsCSSRule.h"
#include "nsICSSStyleRule.h"
#include "nsICSSGroupRule.h"
#include "nsCSSDeclaration.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSParser.h"
#include "nsICSSLoader.h"
#include "nsIURL.h"
#include "nsPresContext.h"
#include "nsIDocument.h"
#include "nsIDeviceContext.h"
#include "nsIAtom.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsStyleConsts.h"
#include "nsStyleUtil.h"
#include "nsIFontMetrics.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsICSSStyleRuleDOMWrapper.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsDOMCSSDeclaration.h"
#include "nsINameSpaceManager.h"
#include "nsXMLNameSpaceMap.h"
#include "nsILookAndFeel.h"
#include "nsRuleNode.h"
#include "nsUnicharUtils.h"
#include "nsCSSPseudoElements.h"
#include "nsIPrincipal.h"
#include "nsComponentManagerUtils.h"
#include "nsCSSPseudoClasses.h"

#include "nsContentUtils.h"
#include "nsContentErrors.h"
#include "mozAutoDocUpdate.h"

#define NS_IF_CLONE(member_)                                                  \
  PR_BEGIN_MACRO                                                              \
    if (member_) {                                                            \
      result->member_ = member_->Clone();                                     \
      if (!result->member_) {                                                 \
        delete result;                                                        \
        return nsnull;                                                        \
      }                                                                       \
    }                                                                         \
  PR_END_MACRO

#define NS_IF_DELETE(ptr)                                                     \
  PR_BEGIN_MACRO                                                              \
    delete ptr;                                                               \
    ptr = nsnull;                                                             \
  PR_END_MACRO

/* ************************************************************************** */

nsAtomList::nsAtomList(nsIAtom* aAtom)
  : mAtom(aAtom),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAtomList);
}

nsAtomList::nsAtomList(const nsString& aAtomValue)
  : mAtom(nsnull),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAtomList);
  mAtom = do_GetAtom(aAtomValue);
}

nsAtomList*
nsAtomList::Clone(PRBool aDeep) const
{
  nsAtomList *result = new nsAtomList(mAtom);
  if (!result)
    return nsnull;

  if (aDeep)
    NS_CSS_CLONE_LIST_MEMBER(nsAtomList, this, mNext, result, (PR_FALSE));
  return result;
}

nsAtomList::~nsAtomList(void)
{
  MOZ_COUNT_DTOR(nsAtomList);
  NS_CSS_DELETE_LIST_MEMBER(nsAtomList, this, mNext);
}

nsPseudoClassList::nsPseudoClassList(nsIAtom* aAtom)
  : mAtom(aAtom),
    mNext(nsnull)
{
  NS_ASSERTION(!nsCSSPseudoClasses::HasStringArg(aAtom) &&
               !nsCSSPseudoClasses::HasNthPairArg(aAtom),
               "unexpected pseudo-class");
  MOZ_COUNT_CTOR(nsPseudoClassList);
  u.mMemory = nsnull;
}

nsPseudoClassList::nsPseudoClassList(nsIAtom* aAtom, const PRUnichar* aString)
  : mAtom(aAtom),
    mNext(nsnull)
{
  NS_ASSERTION(nsCSSPseudoClasses::HasStringArg(aAtom),
               "unexpected pseudo-class");
  NS_ASSERTION(aString, "string expected");
  MOZ_COUNT_CTOR(nsPseudoClassList);
  u.mString = NS_strdup(aString);
}

nsPseudoClassList::nsPseudoClassList(nsIAtom* aAtom, const PRInt32* aIntPair)
  : mAtom(aAtom),
    mNext(nsnull)
{
  NS_ASSERTION(nsCSSPseudoClasses::HasNthPairArg(aAtom),
               "unexpected pseudo-class");
  NS_ASSERTION(aIntPair, "integer pair expected");
  MOZ_COUNT_CTOR(nsPseudoClassList);
  u.mNumbers =
    static_cast<PRInt32*>(nsMemory::Clone(aIntPair, sizeof(PRInt32) * 2));
}

nsPseudoClassList*
nsPseudoClassList::Clone(PRBool aDeep) const
{
  nsPseudoClassList *result;
  if (!u.mMemory) {
    result = new nsPseudoClassList(mAtom);
  } else if (nsCSSPseudoClasses::HasStringArg(mAtom)) {
    result = new nsPseudoClassList(mAtom, u.mString);
  } else {
    NS_ASSERTION(nsCSSPseudoClasses::HasNthPairArg(mAtom),
                 "unexpected pseudo-class");
    result = new nsPseudoClassList(mAtom, u.mNumbers);
  }

  if (aDeep)
    NS_CSS_CLONE_LIST_MEMBER(nsPseudoClassList, this, mNext, result,
                             (PR_FALSE));

  return result;
}

nsPseudoClassList::~nsPseudoClassList(void)
{
  MOZ_COUNT_DTOR(nsPseudoClassList);
  if (u.mMemory)
    NS_Free(u.mMemory);
  NS_CSS_DELETE_LIST_MEMBER(nsPseudoClassList, this, mNext);
}

nsAttrSelector::nsAttrSelector(PRInt32 aNameSpace, const nsString& aAttr)
  : mNameSpace(aNameSpace),
    mAttr(nsnull),
    mFunction(NS_ATTR_FUNC_SET),
    mCaseSensitive(1),
    mValue(),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAttrSelector);

  mAttr = do_GetAtom(aAttr);
}

nsAttrSelector::nsAttrSelector(PRInt32 aNameSpace, const nsString& aAttr, PRUint8 aFunction, 
                               const nsString& aValue, PRBool aCaseSensitive)
  : mNameSpace(aNameSpace),
    mAttr(nsnull),
    mFunction(aFunction),
    mCaseSensitive(aCaseSensitive),
    mValue(aValue),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAttrSelector);

  mAttr = do_GetAtom(aAttr);
}

nsAttrSelector::nsAttrSelector(PRInt32 aNameSpace, nsIAtom* aAttr,
                               PRUint8 aFunction, const nsString& aValue,
                               PRBool aCaseSensitive)
  : mNameSpace(aNameSpace),
    mAttr(aAttr),
    mFunction(aFunction),
    mCaseSensitive(aCaseSensitive),
    mValue(aValue),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAttrSelector);
}

nsAttrSelector*
nsAttrSelector::Clone(PRBool aDeep) const
{
  nsAttrSelector *result =
    new nsAttrSelector(mNameSpace, mAttr, mFunction, mValue, mCaseSensitive);

  if (aDeep)
    NS_CSS_CLONE_LIST_MEMBER(nsAttrSelector, this, mNext, result, (PR_FALSE));

  return result;
}

nsAttrSelector::~nsAttrSelector(void)
{
  MOZ_COUNT_DTOR(nsAttrSelector);

  NS_CSS_DELETE_LIST_MEMBER(nsAttrSelector, this, mNext);
}

// -- nsCSSSelector -------------------------------

nsCSSSelector::nsCSSSelector(void)
  : mNameSpace(kNameSpaceID_Unknown), mTag(nsnull), 
    mIDList(nsnull), 
    mClassList(nsnull), 
    mPseudoClassList(nsnull),
    mAttrList(nsnull), 
    mOperator(0),
    mNegations(nsnull),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSSelector);
}

nsCSSSelector*
nsCSSSelector::Clone(PRBool aDeepNext, PRBool aDeepNegations) const
{
  nsCSSSelector *result = new nsCSSSelector();
  if (!result)
    return nsnull;

  result->mNameSpace = mNameSpace;
  result->mTag = mTag;
  
  NS_IF_CLONE(mIDList);
  NS_IF_CLONE(mClassList);
  NS_IF_CLONE(mPseudoClassList);
  NS_IF_CLONE(mAttrList);

  // No need to worry about multiple levels of recursion since an
  // mNegations can't have an mNext.
  NS_ASSERTION(!mNegations || !mNegations->mNext,
               "mNegations can't have non-null mNext");
  if (aDeepNegations) {
    NS_CSS_CLONE_LIST_MEMBER(nsCSSSelector, this, mNegations, result,
                             (PR_TRUE, PR_FALSE));
  }

  if (aDeepNext) {
    NS_CSS_CLONE_LIST_MEMBER(nsCSSSelector, this, mNext, result,
                             (PR_FALSE, PR_TRUE));
  }

  return result;
}

nsCSSSelector::~nsCSSSelector(void)  
{
  MOZ_COUNT_DTOR(nsCSSSelector);
  Reset();
  // No need to worry about multiple levels of recursion since an
  // mNegations can't have an mNext.
  NS_CSS_DELETE_LIST_MEMBER(nsCSSSelector, this, mNext);
}

void nsCSSSelector::Reset(void)
{
  mNameSpace = kNameSpaceID_Unknown;
  mTag = nsnull;
  NS_IF_DELETE(mIDList);
  NS_IF_DELETE(mClassList);
  NS_IF_DELETE(mPseudoClassList);
  NS_IF_DELETE(mAttrList);
  // No need to worry about multiple levels of recursion since an
  // mNegations can't have an mNext.
  NS_ASSERTION(!mNegations || !mNegations->mNext,
               "mNegations can't have non-null mNext");
  NS_CSS_DELETE_LIST_MEMBER(nsCSSSelector, this, mNegations);
  mOperator = PRUnichar(0);
}

void nsCSSSelector::SetNameSpace(PRInt32 aNameSpace)
{
  mNameSpace = aNameSpace;
}

void nsCSSSelector::SetTag(const nsString& aTag)
{
  if (aTag.IsEmpty())
    mTag = nsnull;
  else
    mTag = do_GetAtom(aTag);
}

void nsCSSSelector::AddID(const nsString& aID)
{
  if (!aID.IsEmpty()) {
    nsAtomList** list = &mIDList;
    while (nsnull != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAtomList(aID);
  }
}

void nsCSSSelector::AddClass(const nsString& aClass)
{
  if (!aClass.IsEmpty()) {
    nsAtomList** list = &mClassList;
    while (nsnull != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAtomList(aClass);
  }
}

void nsCSSSelector::AddPseudoClass(nsIAtom* aPseudoClass)
{
  AddPseudoClassInternal(new nsPseudoClassList(aPseudoClass));
}

void nsCSSSelector::AddPseudoClass(nsIAtom* aPseudoClass,
                                   const PRUnichar* aString)
{
  AddPseudoClassInternal(new nsPseudoClassList(aPseudoClass, aString));
}

void nsCSSSelector::AddPseudoClass(nsIAtom* aPseudoClass,
                                   const PRInt32* aIntPair)
{
  AddPseudoClassInternal(new nsPseudoClassList(aPseudoClass, aIntPair));
}

void nsCSSSelector::AddPseudoClassInternal(nsPseudoClassList *aPseudoClass)
{
  nsPseudoClassList** list = &mPseudoClassList;
  while (nsnull != *list) {
    list = &((*list)->mNext);
  }
  *list = aPseudoClass;
}

void nsCSSSelector::AddAttribute(PRInt32 aNameSpace, const nsString& aAttr)
{
  if (!aAttr.IsEmpty()) {
    nsAttrSelector** list = &mAttrList;
    while (nsnull != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAttrSelector(aNameSpace, aAttr);
  }
}

void nsCSSSelector::AddAttribute(PRInt32 aNameSpace, const nsString& aAttr, PRUint8 aFunc, 
                                 const nsString& aValue, PRBool aCaseSensitive)
{
  if (!aAttr.IsEmpty()) {
    nsAttrSelector** list = &mAttrList;
    while (nsnull != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAttrSelector(aNameSpace, aAttr, aFunc, aValue, aCaseSensitive);
  }
}

void nsCSSSelector::SetOperator(PRUnichar aOperator)
{
  mOperator = aOperator;
}

PRInt32 nsCSSSelector::CalcWeight(void) const
{
  PRInt32 weight = 0;

  if (nsnull != mTag) {
    weight += 0x000001;
  }
  nsAtomList* list = mIDList;
  while (nsnull != list) {
    weight += 0x010000;
    list = list->mNext;
  }
  list = mClassList;
  while (nsnull != list) {
    weight += 0x000100;
    list = list->mNext;
  }
  nsPseudoClassList *plist = mPseudoClassList;
  while (nsnull != plist) {
    weight += 0x000100;
    plist = plist->mNext;
  }
  nsAttrSelector* attr = mAttrList;
  while (nsnull != attr) {
    weight += 0x000100;
    attr = attr->mNext;
  }
  if (nsnull != mNegations) {
    weight += mNegations->CalcWeight();
  }
  return weight;
}

// pseudo-elements are stored in the selectors' chain using fictional elements;
// these fictional elements have mTag starting with a colon
static PRBool IsPseudoElement(nsIAtom* aAtom)
{
  if (aAtom) {
    const char* str;
    aAtom->GetUTF8String(&str);
    return str && (*str == ':');
  }

  return PR_FALSE;
}

void nsCSSSelector::AppendNegationToString(nsAString& aString)
{
  aString.AppendLiteral(":not(");
}

//
// Builds the textual representation of a selector. Called by DOM 2 CSS 
// StyleRule:selectorText
//
void
nsCSSSelector::ToString(nsAString& aString, nsICSSStyleSheet* aSheet,
                        PRBool aAppend) const
{
  if (!aAppend)
   aString.Truncate();
   
  ToStringInternal(aString, aSheet, IsPseudoElement(mTag), PR_FALSE);
}

void nsCSSSelector::ToStringInternal(nsAString& aString,
                                     nsICSSStyleSheet* aSheet,
                                     PRBool aIsPseudoElem,
                                     PRBool aIsNegated) const
{
  nsAutoString temp;
  PRBool isPseudoElement = IsPseudoElement(mTag);
  
  // selectors are linked from right-to-left, so the next selector in the linked list
  // actually precedes this one in the resulting string
  if (mNext) {
    mNext->ToStringInternal(aString, aSheet, IsPseudoElement(mTag), 0);
    if (!aIsNegated && !isPseudoElement) {
      // don't add a leading whitespace if we have a pseudo-element
      // or a negated simple selector
      aString.Append(PRUnichar(' '));
    }
  }

  // For non-pseudo-element selectors or for lone pseudo-elements, deal with
  // namespace prefixes.
  PRBool wroteNamespace = PR_FALSE;
  if (!isPseudoElement || !mNext) {
    // append the namespace prefix if needed
    nsXMLNameSpaceMap *sheetNS = aSheet ? aSheet->GetNameSpaceMap() : nsnull;

    // sheetNS is non-null if and only if we had an @namespace rule.  If it's
    // null, that means that the only namespaces we could have are the
    // wildcard namespace (which can be implicit in this case) and the "none"
    // namespace, which then needs to be explicitly specified.
    if (!sheetNS) {
      NS_ASSERTION(mNameSpace == kNameSpaceID_Unknown ||
                   mNameSpace == kNameSpaceID_None,
                   "How did we get this namespace?");
      if (mNameSpace == kNameSpaceID_None) {
        aString.Append(PRUnichar('|'));
        wroteNamespace = PR_TRUE;
      }
    } else if (sheetNS->FindNameSpaceID(nsnull) == mNameSpace) {
      // We have the default namespace (possibly including the wildcard
      // namespace).  Do nothing.
      NS_ASSERTION(mNameSpace == kNameSpaceID_Unknown ||
                   CanBeNamespaced(aIsNegated),
                   "How did we end up with this namespace?");
    } else if (mNameSpace != kNameSpaceID_Unknown) {
      NS_ASSERTION(CanBeNamespaced(aIsNegated),
                   "How did we end up with this namespace?");
      nsIAtom *prefixAtom = sheetNS->FindPrefix(mNameSpace);
      NS_ASSERTION(prefixAtom, "how'd we get a non-default namespace "
                   "without a prefix?");
      nsAutoString prefix;
      prefixAtom->ToString(prefix);
      aString.Append(prefix);
      aString.Append(PRUnichar('|'));
      wroteNamespace = PR_TRUE;
    } else {
      // A selector for an element in any namespace, while the default
      // namespace is something else.  :not() is special in that the default
      // namespace is not implied for non-type selectors, so if this is a
      // negated non-type selector we don't need to output an explicit wildcard
      // namespace here, since those default to a wildcard namespace.
      if (CanBeNamespaced(aIsNegated)) {
        aString.AppendLiteral("*|");
        wroteNamespace = PR_TRUE;
      }
    }
  }
      
  if (!mTag) {
    // Universal selector:  avoid writing the universal selector when we
    // can avoid it, especially since we're required to avoid it for the
    // inside of :not()
    if (wroteNamespace ||
        (!mIDList && !mClassList && !mPseudoClassList && !mAttrList &&
         (aIsNegated || !mNegations))) {
      aString.Append(PRUnichar('*'));
    }
  } else {
    // Append the tag name
    if (isPseudoElement) {
      if (!mNext) {
        // Lone pseudo-element selector -- toss in a wildcard type selector
        // XXXldb Why?
        aString.Append(PRUnichar('*'));
      }
      if (!nsCSSPseudoElements::IsCSS2PseudoElement(mTag)) {
        aString.Append(PRUnichar(':'));
      }
    }
    nsAutoString prefix;
    mTag->ToString(prefix);
    aString.Append(prefix);
  }

  // Append the id, if there is one
  if (mIDList) {
    nsAtomList* list = mIDList;
    while (list != nsnull) {
      list->mAtom->ToString(temp);
      aString.Append(PRUnichar('#'));
      aString.Append(temp);
      list = list->mNext;
    }
  }

  // Append each class in the linked list
  if (mClassList) {
    nsAtomList* list = mClassList;
    while (list != nsnull) {
      list->mAtom->ToString(temp);
      aString.Append(PRUnichar('.'));
      aString.Append(temp);
      list = list->mNext;
    }
  }

  // Append each attribute selector in the linked list
  if (mAttrList) {
    nsAttrSelector* list = mAttrList;
    while (list != nsnull) {
      aString.Append(PRUnichar('['));
      // Append the namespace prefix
      if (list->mNameSpace > 0) {
        if (aSheet) {
          nsXMLNameSpaceMap *sheetNS = aSheet->GetNameSpaceMap();
          // will return null if namespace was the default
          nsIAtom *prefixAtom = sheetNS->FindPrefix(list->mNameSpace);
          if (prefixAtom) { 
            nsAutoString prefix;
            prefixAtom->ToString(prefix);
            aString.Append(prefix);
            aString.Append(PRUnichar('|'));
          }
        }
      }
      // Append the attribute name
      list->mAttr->ToString(temp);
      aString.Append(temp);

      if (list->mFunction != NS_ATTR_FUNC_SET) {
        // Append the function
        if (list->mFunction == NS_ATTR_FUNC_INCLUDES)
          aString.Append(PRUnichar('~'));
        else if (list->mFunction == NS_ATTR_FUNC_DASHMATCH)
          aString.Append(PRUnichar('|'));
        else if (list->mFunction == NS_ATTR_FUNC_BEGINSMATCH)
          aString.Append(PRUnichar('^'));
        else if (list->mFunction == NS_ATTR_FUNC_ENDSMATCH)
          aString.Append(PRUnichar('$'));
        else if (list->mFunction == NS_ATTR_FUNC_CONTAINSMATCH)
          aString.Append(PRUnichar('*'));

        aString.Append(PRUnichar('='));
      
        // Append the value
        nsAutoString escaped;
        nsStyleUtil::EscapeCSSString(list->mValue, escaped);
      
        aString.Append(PRUnichar('\"'));
        aString.Append(escaped);
        aString.Append(PRUnichar('\"'));
      }

      aString.Append(PRUnichar(']'));
      
      list = list->mNext;
    }
  }

  // Append each pseudo-class in the linked list
  if (mPseudoClassList) {
    nsPseudoClassList* list = mPseudoClassList;
    while (list != nsnull) {
      list->mAtom->ToString(temp);
      aString.Append(temp);
      if (list->u.mMemory) {
        aString.Append(PRUnichar('('));
        if (nsCSSPseudoClasses::HasStringArg(list->mAtom)) {
          aString.Append(list->u.mString);
        } else {
          NS_ASSERTION(nsCSSPseudoClasses::HasNthPairArg(list->mAtom),
                       "unexpected pseudo-class");
          PRInt32 a = list->u.mNumbers[0],
                  b = list->u.mNumbers[1];
          temp.Truncate();
          if (a != 0) {
            if (a == -1) {
              temp.Append(PRUnichar('-'));
            } else if (a != 1) {
              temp.AppendInt(a);
            }
            temp.Append(PRUnichar('n'));
          }
          if (b != 0 || a == 0) {
            if (b >= 0 && a != 0) // check a != 0 for whether we printed above
              temp.Append(PRUnichar('+'));
            temp.AppendInt(b);
          }
          aString.Append(temp);
        }
        aString.Append(PRUnichar(')'));
      }
      list = list->mNext;
    }
  }

  if (!aIsNegated) {
    for (nsCSSSelector* negation = mNegations; negation;
         negation = negation->mNegations) {
      aString.AppendLiteral(":not(");
      negation->ToStringInternal(aString, aSheet, PR_FALSE, PR_TRUE);
      aString.Append(PRUnichar(')'));
    }
  }

  // Append the operator only if the selector is not negated and is not
  // a pseudo-element
  if (!aIsNegated && mOperator && !aIsPseudoElem) {
    aString.Append(PRUnichar(' '));
    aString.Append(mOperator);
  }
}

PRBool
nsCSSSelector::CanBeNamespaced(PRBool aIsNegated) const
{
  return !aIsNegated ||
         (!mIDList && !mClassList && !mPseudoClassList && !mAttrList);
}

// -- nsCSSSelectorList -------------------------------

nsCSSSelectorList::nsCSSSelectorList(void)
  : mSelectors(nsnull),
    mWeight(0),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSSelectorList);
}

nsCSSSelectorList::~nsCSSSelectorList()
{
  MOZ_COUNT_DTOR(nsCSSSelectorList);
  delete mSelectors;
  NS_CSS_DELETE_LIST_MEMBER(nsCSSSelectorList, this, mNext);
}

void nsCSSSelectorList::AddSelector(nsAutoPtr<nsCSSSelector>& aSelector)
{ // prepend to list
  nsCSSSelector* newSel = aSelector.forget();
  if (newSel) {
    newSel->mNext = mSelectors;
    mSelectors = newSel;
  }
}

void
nsCSSSelectorList::ToString(nsAString& aResult, nsICSSStyleSheet* aSheet)
{
  aResult.Truncate();
  nsCSSSelectorList *p = this;
  for (;;) {
    p->mSelectors->ToString(aResult, aSheet, PR_TRUE);
    p = p->mNext;
    if (!p)
      break;
    aResult.AppendLiteral(", ");
  }
}

nsCSSSelectorList*
nsCSSSelectorList::Clone(PRBool aDeep) const
{
  nsCSSSelectorList *result = new nsCSSSelectorList();
  result->mWeight = mWeight;
  NS_IF_CLONE(mSelectors);

  if (aDeep) {
    NS_CSS_CLONE_LIST_MEMBER(nsCSSSelectorList, this, mNext, result,
                             (PR_FALSE));
  }
  return result;
}

// -- CSSImportantRule -------------------------------

class CSSStyleRuleImpl;

class CSSImportantRule : public nsIStyleRule {
public:
  CSSImportantRule(nsCSSDeclaration* aDeclaration);

  NS_DECL_ISUPPORTS

  // nsIStyleRule interface
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);
#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

protected:
  virtual ~CSSImportantRule(void);

  nsCSSDeclaration*  mDeclaration;

  friend class CSSStyleRuleImpl;
};

CSSImportantRule::CSSImportantRule(nsCSSDeclaration* aDeclaration)
  : mDeclaration(aDeclaration)
{
}

CSSImportantRule::~CSSImportantRule(void)
{
  mDeclaration = nsnull;
}

NS_IMPL_ISUPPORTS1(CSSImportantRule, nsIStyleRule)

NS_IMETHODIMP
CSSImportantRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  // Check this at runtime because it might be hit in some out-of-memory cases.
  NS_ENSURE_TRUE(mDeclaration->HasImportantData(), NS_ERROR_UNEXPECTED);

  return mDeclaration->MapImportantRuleInfoInto(aRuleData);
}

#ifdef DEBUG
NS_IMETHODIMP
CSSImportantRule::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("! Important rule ", out);
  if (nsnull != mDeclaration) {
    mDeclaration->List(out);
  }
  else {
    fputs("{ null declaration }", out);
  }
  fputs("\n", out);

  return NS_OK;
}
#endif

// --------------------------------------------------------

class DOMCSSStyleRuleImpl;

class DOMCSSDeclarationImpl : public nsDOMCSSDeclaration
{
public:
  DOMCSSDeclarationImpl(nsICSSStyleRule *aRule);
  virtual ~DOMCSSDeclarationImpl(void);

  NS_IMETHOD GetParentRule(nsIDOMCSSRule **aParent);
  virtual void DropReference(void);
  virtual nsresult GetCSSDeclaration(nsCSSDeclaration **aDecl,
                                     PRBool aAllocate);
  virtual nsresult GetCSSParsingEnvironment(nsIURI** aSheetURI,
                                            nsIURI** aBaseURI,
                                            nsIPrincipal** aSheetPrincipal,
                                            nsICSSLoader** aCSSLoader,
                                            nsICSSParser** aCSSParser);
  virtual nsresult DeclarationChanged();

  // Override |AddRef| and |Release| for being a member of
  // |DOMCSSStyleRuleImpl|.
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  friend class DOMCSSStyleRuleImpl;

protected:
  // This reference is not reference-counted. The rule object tells us
  // when it's about to go away.
  nsICSSStyleRule *mRule;

  inline DOMCSSStyleRuleImpl* DomRule();

private:
  // NOT TO BE IMPLEMENTED
  // This object cannot be allocated on its own.  It must be a member of
  // DOMCSSStyleRuleImpl.
  void* operator new(size_t size) CPP_THROW_NEW;
};

class DOMCSSStyleRuleImpl : public nsICSSStyleRuleDOMWrapper
{
public:
  DOMCSSStyleRuleImpl(nsICSSStyleRule *aRule);
  virtual ~DOMCSSStyleRuleImpl();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMCSSRULE
  NS_DECL_NSIDOMCSSSTYLERULE

  // nsICSSStyleRuleDOMWrapper
  NS_IMETHOD GetCSSStyleRule(nsICSSStyleRule **aResult);

  DOMCSSDeclarationImpl* DOMDeclaration() { return &mDOMDeclaration; }

  friend class DOMCSSDeclarationImpl;

protected:
  DOMCSSDeclarationImpl mDOMDeclaration;

  nsICSSStyleRule* Rule() {
    return mDOMDeclaration.mRule;
  }
};

DOMCSSDeclarationImpl::DOMCSSDeclarationImpl(nsICSSStyleRule *aRule)
  : mRule(aRule)
{
  MOZ_COUNT_CTOR(DOMCSSDeclarationImpl);
}

DOMCSSDeclarationImpl::~DOMCSSDeclarationImpl(void)
{
  NS_ASSERTION(!mRule, "DropReference not called.");

  MOZ_COUNT_DTOR(DOMCSSDeclarationImpl);
}

inline DOMCSSStyleRuleImpl* DOMCSSDeclarationImpl::DomRule()
{
  return reinterpret_cast<DOMCSSStyleRuleImpl*>
                         (reinterpret_cast<char*>(this) -
           offsetof(DOMCSSStyleRuleImpl, mDOMDeclaration));
}

NS_IMPL_ADDREF_USING_AGGREGATOR(DOMCSSDeclarationImpl, DomRule())
NS_IMPL_RELEASE_USING_AGGREGATOR(DOMCSSDeclarationImpl, DomRule())

void
DOMCSSDeclarationImpl::DropReference(void)
{
  mRule = nsnull;
}

nsresult
DOMCSSDeclarationImpl::GetCSSDeclaration(nsCSSDeclaration **aDecl,
                                         PRBool aAllocate)
{
  if (mRule) {
    *aDecl = mRule->GetDeclaration();
  }
  else {
    *aDecl = nsnull;
  }

  return NS_OK;
}

/*
 * This is a utility function.  It will only fail if it can't get a
 * parser.  This means it can return NS_OK without aURI or aCSSLoader
 * being initialized.
 */
nsresult
DOMCSSDeclarationImpl::GetCSSParsingEnvironment(nsIURI** aSheetURI, 
                                                nsIURI** aBaseURI,
                                                nsIPrincipal** aSheetPrincipal,
                                                nsICSSLoader** aCSSLoader,
                                                nsICSSParser** aCSSParser)
{
  // null out the out params since some of them may not get initialized below
  *aSheetURI = nsnull;
  *aBaseURI = nsnull;
  *aSheetPrincipal = nsnull;
  *aCSSLoader = nsnull;
  *aCSSParser = nsnull;
  nsresult result;
  nsCOMPtr<nsIStyleSheet> sheet;
  if (mRule) {
    mRule->GetStyleSheet(*getter_AddRefs(sheet));
    if (sheet) {
      sheet->GetSheetURI(aSheetURI);
      sheet->GetBaseURI(aBaseURI);

      nsCOMPtr<nsICSSStyleSheet> cssSheet(do_QueryInterface(sheet));
      if (cssSheet) {
        NS_ADDREF(*aSheetPrincipal = cssSheet->Principal());
      }

      nsCOMPtr<nsIDocument> document;
      sheet->GetOwningDocument(*getter_AddRefs(document));
      if (document) {
        NS_ADDREF(*aCSSLoader = document->CSSLoader());
      }
    }
  }
  // XXXldb Why bother if |mRule| is null?
  if (*aCSSLoader) {
    result = (*aCSSLoader)->GetParserFor(nsnull, aCSSParser);
  } else {
    result = NS_NewCSSParser(aCSSParser);
  }

  if (NS_SUCCEEDED(result) && !*aSheetPrincipal) {
    result = CallCreateInstance("@mozilla.org/nullprincipal;1",
                                aSheetPrincipal);
  }

  return result;
}

NS_IMETHODIMP
DOMCSSDeclarationImpl::GetParentRule(nsIDOMCSSRule **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);

  if (!mRule) {
    *aParent = nsnull;
    return NS_OK;
  }

  return mRule->GetDOMRule(aParent);
}

nsresult
DOMCSSDeclarationImpl::DeclarationChanged()
{
  NS_PRECONDITION(mRule,
         "can only be called when |GetCSSDeclaration| returned a declaration");

  nsCOMPtr<nsIDocument> owningDoc;
  nsCOMPtr<nsIStyleSheet> sheet;
  mRule->GetStyleSheet(*getter_AddRefs(sheet));
  if (sheet) {
    sheet->GetOwningDocument(*getter_AddRefs(owningDoc));
  }

  mozAutoDocUpdate updateBatch(owningDoc, UPDATE_STYLE, PR_TRUE);

  nsCOMPtr<nsICSSStyleRule> oldRule = mRule;
  mRule = oldRule->DeclarationChanged(PR_TRUE).get();
  if (!mRule)
    return NS_ERROR_OUT_OF_MEMORY;
  nsrefcnt cnt = mRule->Release();
  if (cnt == 0) {
    NS_NOTREACHED("container didn't take ownership");
    mRule = nsnull;
    return NS_ERROR_UNEXPECTED;
  }

  if (owningDoc) {
    owningDoc->StyleRuleChanged(sheet, oldRule, mRule);
  }
  return NS_OK;
}

DOMCSSStyleRuleImpl::DOMCSSStyleRuleImpl(nsICSSStyleRule* aRule)
  : mDOMDeclaration(aRule)
{
}

DOMCSSStyleRuleImpl::~DOMCSSStyleRuleImpl()
{
}

NS_INTERFACE_MAP_BEGIN(DOMCSSStyleRuleImpl)
  NS_INTERFACE_MAP_ENTRY(nsICSSStyleRuleDOMWrapper)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSStyleRule)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(DOMCSSStyleRuleImpl)
NS_IMPL_RELEASE(DOMCSSStyleRuleImpl)

NS_IMETHODIMP    
DOMCSSStyleRuleImpl::GetType(PRUint16* aType)
{
  *aType = nsIDOMCSSRule::STYLE_RULE;
  
  return NS_OK;
}

NS_IMETHODIMP    
DOMCSSStyleRuleImpl::GetCssText(nsAString& aCssText)
{
  if (!Rule()) {
    aCssText.Truncate();
    return NS_OK;
  }
  return Rule()->GetCssText(aCssText);
}

NS_IMETHODIMP    
DOMCSSStyleRuleImpl::SetCssText(const nsAString& aCssText)
{
  if (!Rule()) {
    return NS_OK;
  }
  return Rule()->SetCssText(aCssText);
}

NS_IMETHODIMP    
DOMCSSStyleRuleImpl::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  if (!Rule()) {
    *aSheet = nsnull;
    return NS_OK;
  }
  nsCOMPtr<nsICSSStyleSheet> sheet;
  Rule()->GetParentStyleSheet(getter_AddRefs(sheet));
  if (!sheet) {
    *aSheet = nsnull;
    return NS_OK;
  }
  return CallQueryInterface(sheet, aSheet);
}

NS_IMETHODIMP    
DOMCSSStyleRuleImpl::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  if (!Rule()) {
    *aParentRule = nsnull;
    return NS_OK;
  }
  nsCOMPtr<nsICSSGroupRule> rule;
  Rule()->GetParentRule(getter_AddRefs(rule));
  if (!rule) {
    *aParentRule = nsnull;
    return NS_OK;
  }
  return rule->GetDOMRule(aParentRule);
}

NS_IMETHODIMP    
DOMCSSStyleRuleImpl::GetSelectorText(nsAString& aSelectorText)
{
  if (!Rule()) {
    aSelectorText.Truncate();
    return NS_OK;
  }
  return Rule()->GetSelectorText(aSelectorText);
}

NS_IMETHODIMP    
DOMCSSStyleRuleImpl::SetSelectorText(const nsAString& aSelectorText)
{
  if (!Rule()) {
    return NS_OK;
  }
  return Rule()->SetSelectorText(aSelectorText);
}

NS_IMETHODIMP    
DOMCSSStyleRuleImpl::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  *aStyle = &mDOMDeclaration;
  NS_ADDREF(*aStyle);
  return NS_OK;
}

NS_IMETHODIMP
DOMCSSStyleRuleImpl::GetCSSStyleRule(nsICSSStyleRule **aResult)
{
  *aResult = Rule();
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

// -- nsCSSStyleRule -------------------------------

class CSSStyleRuleImpl : public nsCSSRule,
                         public nsICSSStyleRule
{
public:
  CSSStyleRuleImpl(nsCSSSelectorList* aSelector,
                   nsCSSDeclaration *aDeclaration);
private:
  // for |Clone|
  CSSStyleRuleImpl(const CSSStyleRuleImpl& aCopy); 
  // for |DeclarationChanged|
  CSSStyleRuleImpl(CSSStyleRuleImpl& aCopy,
                   nsCSSDeclaration *aDeclaration); 
public:

  NS_DECL_ISUPPORTS_INHERITED

  virtual nsCSSSelectorList* Selector(void);

  virtual PRUint32 GetLineNumber(void) const;
  virtual void SetLineNumber(PRUint32 aLineNumber);

  virtual nsCSSDeclaration* GetDeclaration(void) const;

  virtual already_AddRefed<nsIStyleRule> GetImportantRule(void);

  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
  NS_IMETHOD SetStyleSheet(nsICSSStyleSheet* aSheet);
  
  NS_IMETHOD SetParentRule(nsICSSGroupRule* aRule);

  virtual nsresult GetCssText(nsAString& aCssText);
  virtual nsresult SetCssText(const nsAString& aCssText);
  virtual nsresult GetParentStyleSheet(nsICSSStyleSheet** aSheet);
  virtual nsresult GetParentRule(nsICSSGroupRule** aParentRule);
  virtual nsresult GetSelectorText(nsAString& aSelectorText);
  virtual nsresult SetSelectorText(const nsAString& aSelectorText);

  NS_IMETHOD GetType(PRInt32& aType) const;
  NS_IMETHOD Clone(nsICSSRule*& aClone) const;

  nsIDOMCSSRule* GetDOMRuleWeak(nsresult* aResult);

  virtual already_AddRefed<nsICSSStyleRule>
    DeclarationChanged(PRBool aHandleContainer);

  // The new mapping function.
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

private: 
  // These are not supported and are not implemented! 
  CSSStyleRuleImpl& operator=(const CSSStyleRuleImpl& aCopy); 

protected:
  virtual ~CSSStyleRuleImpl(void);

protected:
  nsCSSSelectorList*      mSelector; // null for style attribute
  nsCSSDeclaration*       mDeclaration;
  CSSImportantRule*       mImportantRule;
  DOMCSSStyleRuleImpl*    mDOMRule;                          
  PRUint32                mLineNumber;
};

CSSStyleRuleImpl::CSSStyleRuleImpl(nsCSSSelectorList* aSelector,
                                   nsCSSDeclaration* aDeclaration)
  : nsCSSRule(),
    mSelector(aSelector),
    mDeclaration(aDeclaration), 
    mImportantRule(nsnull),
    mDOMRule(nsnull),
    mLineNumber(0)
{
  mDeclaration->AddRef();
}

// for |Clone|
CSSStyleRuleImpl::CSSStyleRuleImpl(const CSSStyleRuleImpl& aCopy)
  : nsCSSRule(aCopy),
    mSelector(aCopy.mSelector ? aCopy.mSelector->Clone() : nsnull),
    mDeclaration(aCopy.mDeclaration->Clone()),
    mImportantRule(nsnull),
    mDOMRule(nsnull),
    mLineNumber(aCopy.mLineNumber)
{
  if (mDeclaration)
    mDeclaration->AddRef();
  // rest is constructed lazily on existing data
}

// for |DeclarationChanged|
CSSStyleRuleImpl::CSSStyleRuleImpl(CSSStyleRuleImpl& aCopy,
                                   nsCSSDeclaration* aDeclaration)
  : nsCSSRule(aCopy),
    mSelector(aCopy.mSelector),
    mDeclaration(aDeclaration),
    mImportantRule(nsnull),
    mDOMRule(aCopy.mDOMRule),
    mLineNumber(aCopy.mLineNumber)
{
  // The DOM rule is replacing |aCopy| with |this|, so transfer
  // the reverse pointer as well (and transfer ownership).
  aCopy.mDOMRule = nsnull;

  NS_ASSERTION(aDeclaration == aCopy.mDeclaration, "declaration mismatch");
  // Transfer ownership of selector and declaration:
  aCopy.mSelector = nsnull;
#if 0
  aCopy.mDeclaration = nsnull;
#else
  // We ought to be able to transfer ownership of the selector and the
  // declaration since this rule should now be unused, but unfortunately
  // SetInlineStyleRule might use it before setting the new rule (see
  // stack in bug 209575).  So leave the declaration pointer on the old
  // rule.
  mDeclaration->AddRef();
#endif
}


CSSStyleRuleImpl::~CSSStyleRuleImpl(void)
{
  if (mSelector) {
    delete mSelector;
    mSelector = nsnull;
  }
  if (nsnull != mDeclaration) {
    mDeclaration->Release();
    mDeclaration = nsnull;
  }
  if (nsnull != mImportantRule) {
    NS_RELEASE(mImportantRule);
    mImportantRule = nsnull;
  }
  if (mDOMRule) {
    mDOMRule->DOMDeclaration()->DropReference();
    NS_RELEASE(mDOMRule);
  }
}

// QueryInterface implementation for CSSStyleRuleImpl
NS_INTERFACE_MAP_BEGIN(CSSStyleRuleImpl)
  NS_INTERFACE_MAP_ENTRY(nsICSSStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsICSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICSSStyleRule)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF_INHERITED(CSSStyleRuleImpl, nsCSSRule)
NS_IMPL_RELEASE_INHERITED(CSSStyleRuleImpl, nsCSSRule)

nsCSSSelectorList* CSSStyleRuleImpl::Selector(void)
{
  return mSelector;
}

PRUint32 CSSStyleRuleImpl::GetLineNumber(void) const
{
  return mLineNumber;
}

void CSSStyleRuleImpl::SetLineNumber(PRUint32 aLineNumber)
{
  mLineNumber = aLineNumber;
}

nsCSSDeclaration* CSSStyleRuleImpl::GetDeclaration(void) const
{
  return mDeclaration;
}

already_AddRefed<nsIStyleRule> CSSStyleRuleImpl::GetImportantRule(void)
{
  if (!mDeclaration->HasImportantData()) {
    NS_ASSERTION(!mImportantRule, "immutable, so should be no important rule");
    return nsnull;
  }

  if (!mImportantRule) {
    mImportantRule = new CSSImportantRule(mDeclaration);
    if (!mImportantRule)
      return nsnull;
    NS_ADDREF(mImportantRule);
  }
  NS_ADDREF(mImportantRule);
  return mImportantRule;
}

NS_IMETHODIMP
CSSStyleRuleImpl::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
// XXX What about inner, etc.
  return nsCSSRule::GetStyleSheet(aSheet);
}

NS_IMETHODIMP
CSSStyleRuleImpl::SetStyleSheet(nsICSSStyleSheet* aSheet)
{
  return nsCSSRule::SetStyleSheet(aSheet);
}

NS_IMETHODIMP
CSSStyleRuleImpl::SetParentRule(nsICSSGroupRule* aRule)
{
  return nsCSSRule::SetParentRule(aRule);
}

NS_IMETHODIMP
CSSStyleRuleImpl::GetType(PRInt32& aType) const
{
  aType = nsICSSRule::STYLE_RULE;
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleRuleImpl::Clone(nsICSSRule*& aClone) const
{
  CSSStyleRuleImpl* clone = new CSSStyleRuleImpl(*this);
  if (!clone || !clone->mDeclaration || (!clone->mSelector != !mSelector)) {
    delete clone;
    aClone = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return CallQueryInterface(clone, &aClone);
}

nsIDOMCSSRule*
CSSStyleRuleImpl::GetDOMRuleWeak(nsresult *aResult)
{
  *aResult = NS_OK;
  if (!mSheet) {
    // inline style rules aren't supposed to have a DOM rule object, only
    // a declaration.
    return nsnull;
  }
  if (!mDOMRule) {
    mDOMRule = new DOMCSSStyleRuleImpl(this);
    if (!mDOMRule) {
      *aResult = NS_ERROR_OUT_OF_MEMORY;
      return nsnull;
    }
    NS_ADDREF(mDOMRule);
  }
  return mDOMRule;
}

/* virtual */ already_AddRefed<nsICSSStyleRule>
CSSStyleRuleImpl::DeclarationChanged(PRBool aHandleContainer)
{
  CSSStyleRuleImpl* clone = new CSSStyleRuleImpl(*this, mDeclaration);
  if (!clone) {
    return nsnull;
  }

  NS_ADDREF(clone); // for return

  if (aHandleContainer) {
    NS_ASSERTION(mSheet, "rule must be in a sheet");
    if (mParentRule) {
      mSheet->ReplaceRuleInGroup(mParentRule, this, clone);
    } else {
      mSheet->ReplaceStyleRule(this, clone);
    }
  }

  return clone;
}

NS_IMETHODIMP
CSSStyleRuleImpl::MapRuleInfoInto(nsRuleData* aRuleData)
{
  return mDeclaration->MapRuleInfoInto(aRuleData);
}

#ifdef DEBUG
NS_IMETHODIMP
CSSStyleRuleImpl::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;
  if (mSelector)
    mSelector->ToString(buffer, mSheet);

  buffer.AppendLiteral(" ");
  fputs(NS_LossyConvertUTF16toASCII(buffer).get(), out);
  if (nsnull != mDeclaration) {
    mDeclaration->List(out);
  }
  else {
    fputs("{ null declaration }", out);
  }
  fputs("\n", out);

  return NS_OK;
}
#endif

/* virtual */ nsresult
CSSStyleRuleImpl::GetCssText(nsAString& aCssText)
{
  if (mSelector) {
    mSelector->ToString(aCssText, mSheet);
    aCssText.Append(PRUnichar(' '));
  }
  aCssText.Append(PRUnichar('{'));
  aCssText.Append(PRUnichar(' '));
  if (mDeclaration)
  {
    nsAutoString   tempString;
    mDeclaration->ToString( tempString );
    aCssText.Append( tempString );
  }
  aCssText.Append(PRUnichar(' '));
  aCssText.Append(PRUnichar('}'));
  return NS_OK;
}

/* virtual */ nsresult    
CSSStyleRuleImpl::SetCssText(const nsAString& aCssText)
{
  // XXX TBI - need to re-parse rule & declaration
  return NS_OK;
}

/* virtual */ nsresult    
CSSStyleRuleImpl::GetParentStyleSheet(nsICSSStyleSheet** aSheet)
{
  *aSheet = mSheet;
  NS_IF_ADDREF(*aSheet);
  return NS_OK;
}

/* virtual */ nsresult    
CSSStyleRuleImpl::GetParentRule(nsICSSGroupRule** aParentRule)
{
  *aParentRule = mParentRule;
  NS_IF_ADDREF(*aParentRule);
  return NS_OK;
}

/* virtual */ nsresult    
CSSStyleRuleImpl::GetSelectorText(nsAString& aSelectorText)
{
  if (mSelector)
    mSelector->ToString(aSelectorText, mSheet);
  else
    aSelectorText.Truncate();
  return NS_OK;
}

/* virtual */ nsresult    
CSSStyleRuleImpl::SetSelectorText(const nsAString& aSelectorText)
{
  // XXX TBI - get a parser and re-parse the selectors, 
  // XXX then need to re-compute the cascade
  // XXX and dirty sheet
  return NS_OK;
}

nsresult
NS_NewCSSStyleRule(nsICSSStyleRule** aInstancePtrResult,
                   nsCSSSelectorList* aSelector,
                   nsCSSDeclaration* aDeclaration)
{
  NS_PRECONDITION(aDeclaration, "must have a declaration");
  CSSStyleRuleImpl *it = new CSSStyleRuleImpl(aSelector, aDeclaration);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aInstancePtrResult);
}
