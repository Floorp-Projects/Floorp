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
#include "nsCOMPtr.h"
#include "nsCSSRule.h"
#include "nsICSSStyleRule.h"
#include "nsICSSGroupRule.h"
#include "nsCSSDeclaration.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSParser.h"
#include "nsICSSLoader.h"
#include "nsIURL.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsIDeviceContext.h"
#include "nsIAtom.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsStyleConsts.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIFontMetrics.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsICSSStyleRuleDOMWrapper.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsDOMCSSDeclaration.h"
#include "nsINameSpaceManager.h"
#include "nsINameSpace.h"
#include "nsILookAndFeel.h"
#include "nsRuleNode.h"
#include "nsUnicharUtils.h"
#include "nsCSSPseudoElements.h"

#include "nsContentUtils.h"
#include "nsContentErrors.h"

// -- nsCSSSelector -------------------------------

#define NS_IF_COPY(dest,source,type)  \
  if (nsnull != source)  dest = new type(*(source))

#define NS_IF_DELETE(ptr)   \
  if (nsnull != ptr) { delete ptr; ptr = nsnull; }

#define NS_IF_NEGATED_START(bool,str)  \
  if (bool) { str.Append(NS_LITERAL_STRING(":not(")); }

#define NS_IF_NEGATED_END(bool,str)  \
  if (bool) { str.Append(PRUnichar(')')); }

MOZ_DECL_CTOR_COUNTER(nsAtomList)

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

nsAtomList::nsAtomList(const nsAtomList& aCopy)
  : mAtom(aCopy.mAtom),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAtomList);
  NS_IF_COPY(mNext, aCopy.mNext, nsAtomList);
}

nsAtomList::~nsAtomList(void)
{
  MOZ_COUNT_DTOR(nsAtomList);
  NS_IF_DELETE(mNext);
}

PRBool nsAtomList::Equals(const nsAtomList* aOther) const
{
  if (this == aOther) {
    return PR_TRUE;
  }
  if (nsnull != aOther) {
    if (mAtom == aOther->mAtom) {
      if (nsnull != mNext) {
        return mNext->Equals(aOther->mNext);
      }
      return PRBool(nsnull == aOther->mNext);
    }
  }
  return PR_FALSE;
}

MOZ_DECL_CTOR_COUNTER(nsAtomStringList)

nsAtomStringList::nsAtomStringList(nsIAtom* aAtom, const PRUnichar* aString)
  : mAtom(aAtom),
    mString(nsnull),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAtomStringList);
  if (aString)
    mString = nsCRT::strdup(aString);
}

nsAtomStringList::nsAtomStringList(const nsString& aAtomValue,
                                   const PRUnichar* aString)
  : mAtom(nsnull),
    mString(nsnull),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAtomStringList);
  mAtom = do_GetAtom(aAtomValue);
  if (aString)
    mString = nsCRT::strdup(aString);
}

nsAtomStringList::nsAtomStringList(const nsAtomStringList& aCopy)
  : mAtom(aCopy.mAtom),
    mString(nsnull),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAtomStringList);
  if (aCopy.mString)
    mString = nsCRT::strdup(aCopy.mString);
  NS_IF_COPY(mNext, aCopy.mNext, nsAtomStringList);
}

nsAtomStringList::~nsAtomStringList(void)
{
  MOZ_COUNT_DTOR(nsAtomStringList);
  if (mString)
    nsCRT::free(mString);
  NS_IF_DELETE(mNext);
}

PRBool nsAtomStringList::Equals(const nsAtomStringList* aOther) const
{
  return (this == aOther) ||
         (aOther &&
          mAtom == aOther->mAtom &&
          !mString == !aOther->mString &&
          !mNext == !aOther->mNext &&
          (!mNext || mNext->Equals(aOther->mNext)) &&
          // Check strings last, since it's the slowest check.
          (!mString || nsDependentString(mString).Equals(
                                        nsDependentString(aOther->mString),
                                        nsCaseInsensitiveStringComparator())));
}

MOZ_DECL_CTOR_COUNTER(nsAttrSelector)

nsAttrSelector::nsAttrSelector(PRInt32 aNameSpace, const nsString& aAttr)
  : mNameSpace(aNameSpace),
    mAttr(nsnull),
    mFunction(NS_ATTR_FUNC_SET),
    mCaseSensitive(1),
    mValue(),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAttrSelector);

  mAttr = NS_NewAtom(aAttr);
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

  mAttr = NS_NewAtom(aAttr);
}

nsAttrSelector::nsAttrSelector(const nsAttrSelector& aCopy)
  : mNameSpace(aCopy.mNameSpace),
    mAttr(aCopy.mAttr),
    mFunction(aCopy.mFunction),
    mCaseSensitive(aCopy.mCaseSensitive),
    mValue(aCopy.mValue),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAttrSelector);

  NS_IF_ADDREF(mAttr);
  NS_IF_COPY(mNext, aCopy.mNext, nsAttrSelector);
}

nsAttrSelector::~nsAttrSelector(void)
{
  MOZ_COUNT_DTOR(nsAttrSelector);

  NS_IF_RELEASE(mAttr);
  NS_IF_DELETE(mNext);
}

PRBool nsAttrSelector::Equals(const nsAttrSelector* aOther) const
{
  if (this == aOther) {
    return PR_TRUE;
  }
  if (nsnull != aOther) {
    if ((mNameSpace == aOther->mNameSpace) &&
        (mAttr == aOther->mAttr) && 
        (mFunction == aOther->mFunction) && 
        (mCaseSensitive == aOther->mCaseSensitive) &&
        mValue.Equals(aOther->mValue)) {
      if (nsnull != mNext) {
        return mNext->Equals(aOther->mNext);
      }
      return PRBool(nsnull == aOther->mNext);
    }
  }
  return PR_FALSE;
}

MOZ_DECL_CTOR_COUNTER(nsCSSSelector)

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

nsCSSSelector::nsCSSSelector(const nsCSSSelector& aCopy) 
  : mNameSpace(aCopy.mNameSpace), mTag(aCopy.mTag), 
    mIDList(nsnull), 
    mClassList(nsnull), 
    mPseudoClassList(nsnull),
    mAttrList(nsnull), 
    mOperator(aCopy.mOperator),
    mNegations(nsnull),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSSelector);
  NS_IF_COPY(mIDList, aCopy.mIDList, nsAtomList);
  NS_IF_COPY(mClassList, aCopy.mClassList, nsAtomList);
  NS_IF_COPY(mPseudoClassList, aCopy.mPseudoClassList, nsAtomStringList);
  NS_IF_COPY(mAttrList, aCopy.mAttrList, nsAttrSelector);
  NS_IF_COPY(mNegations, aCopy.mNegations, nsCSSSelector);
}

nsCSSSelector::~nsCSSSelector(void)  
{
  MOZ_COUNT_DTOR(nsCSSSelector);
  Reset();
}

nsCSSSelector& nsCSSSelector::operator=(const nsCSSSelector& aCopy)
{
  NS_IF_DELETE(mIDList);
  NS_IF_DELETE(mClassList);
  NS_IF_DELETE(mPseudoClassList);
  NS_IF_DELETE(mAttrList);
  NS_IF_DELETE(mNegations);
  
  mNameSpace    = aCopy.mNameSpace;
  mTag          = aCopy.mTag;
  NS_IF_COPY(mIDList, aCopy.mIDList, nsAtomList);
  NS_IF_COPY(mClassList, aCopy.mClassList, nsAtomList);
  NS_IF_COPY(mPseudoClassList, aCopy.mPseudoClassList, nsAtomStringList);
  NS_IF_COPY(mAttrList, aCopy.mAttrList, nsAttrSelector);
  mOperator     = aCopy.mOperator;
  NS_IF_COPY(mNegations, aCopy.mNegations, nsCSSSelector);

  return *this;
}

PRBool nsCSSSelector::Equals(const nsCSSSelector* aOther) const
{
  if (this == aOther) {
    return PR_TRUE;
  }
  if (nsnull != aOther) {
    if ((aOther->mNameSpace == mNameSpace) && 
        (aOther->mTag == mTag) && 
        (aOther->mOperator == mOperator)) {
      if (nsnull != mIDList) {
        if (PR_FALSE == mIDList->Equals(aOther->mIDList)) {
          return PR_FALSE;
        }
      }
      else {
        if (nsnull != aOther->mIDList) {
          return PR_FALSE;
        }
      }
      if (nsnull != mClassList) {
        if (PR_FALSE == mClassList->Equals(aOther->mClassList)) {
          return PR_FALSE;
        }
      }
      else {
        if (nsnull != aOther->mClassList) {
          return PR_FALSE;
        }
      }
      if (nsnull != mPseudoClassList) {
        if (PR_FALSE == mPseudoClassList->Equals(aOther->mPseudoClassList)) {
          return PR_FALSE;
        }
      }
      else {
        if (nsnull != aOther->mPseudoClassList) {
          return PR_FALSE;
        }
      }
      if (nsnull != mAttrList) {
        if (PR_FALSE == mAttrList->Equals(aOther->mAttrList)) {
          return PR_FALSE;
        }
      }
      else {
        if (nsnull != aOther->mAttrList) {
          return PR_FALSE;
        }
      }
      if (nsnull != mNegations) {
        if (PR_FALSE == mNegations->Equals(aOther->mNegations)) {
          return PR_FALSE;
        }
      }
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}


void nsCSSSelector::Reset(void)
{
  mNameSpace = kNameSpaceID_Unknown;
  mTag = nsnull;
  NS_IF_DELETE(mIDList);
  NS_IF_DELETE(mClassList);
  NS_IF_DELETE(mPseudoClassList);
  NS_IF_DELETE(mAttrList);
  NS_IF_DELETE(mNegations);
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

void nsCSSSelector::AddPseudoClass(const nsString& aPseudoClass,
                                   const PRUnichar* aString)
{
  if (!aPseudoClass.IsEmpty()) {
    nsAtomStringList** list = &mPseudoClassList;
    while (nsnull != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAtomStringList(aPseudoClass, aString);
  }
}

void nsCSSSelector::AddPseudoClass(nsIAtom* aPseudoClass,
                                   const PRUnichar* aString)
{
  if (nsnull != aPseudoClass) {
    nsAtomStringList** list = &mPseudoClassList;
    while (nsnull != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAtomStringList(aPseudoClass, aString);
  }
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
  nsAtomStringList *plist = mPseudoClassList;
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
  aString.Append(NS_LITERAL_STRING(":not("));
}

//
// Builds the textual representation of a selector. Called by DOM 2 CSS 
// StyleRule:selectorText
//
void
nsCSSSelector::ToString(nsAString& aString, nsICSSStyleSheet* aSheet) const
{
  ToStringInternal(aString, aSheet, IsPseudoElement(mTag), 0);
}

void nsCSSSelector::ToStringInternal(nsAString& aString,
                                     nsICSSStyleSheet* aSheet,
                                     PRBool aIsPseudoElem,
                                     PRIntn aNegatedIndex) const
{
  nsAutoString temp;
  PRBool aIsNegated = PRBool(0 < aNegatedIndex);
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
  if (1 < aNegatedIndex) {
    // the first mNegations does not contain a negated type element selector
    // or a negated universal selector
    NS_IF_NEGATED_START(aIsNegated, aString)
  }

  // For non-pseudo-element selectors or for lone pseudo-elements, deal with
  // namespace prefixes.
  if (!isPseudoElement || !mNext) {
    // append the namespace prefix if needed
    if (mNameSpace == kNameSpaceID_None) {
      // The only way to do this in CSS is to have an explicit namespace
      // of "none" specified in the sheet by having a '|' with nothing
      // before it.
      aString.Append(PRUnichar('|'));
    } else {
      nsCOMPtr<nsINameSpace> sheetNS;
      aSheet->GetNameSpace(*getter_AddRefs(sheetNS));
    
      // sheetNS is non-null if and only if we had an @namespace rule.  If it's
      // null, that means that the only namespaces we could have are the
      // wildcard namespace (which can be implicit in this case) and the "none"
      // namespace, which we handled above.  So no need to output anything when
      // sheetNS is null.
      if (sheetNS) {
        nsCOMPtr<nsIAtom> prefixAtom;
        // prefixAtom is non-null if and only if we have a prefix other than
        // '*'
        if (mNameSpace != kNameSpaceID_Unknown) {
          sheetNS->FindNameSpacePrefix(mNameSpace, getter_AddRefs(prefixAtom));
        }
        if (prefixAtom) {
          nsAutoString prefix;
          prefixAtom->ToString(prefix);
          aString.Append(prefix);
          aString.Append(PRUnichar('|'));
        } else if (mNameSpace == kNameSpaceID_Unknown) {
          // explicit *| or only non-default namespace rules and we're not
          // using any of those namespaces
          aString.Append(PRUnichar('*'));
          aString.Append(PRUnichar('|'));
        }
        // else we are in the default namespace and don't need to output
        // anything
      }
    }
  }
      
  // smells like a universal selector
  if (!mTag && !mIDList && !mClassList) {
    if (1 != aNegatedIndex) {
      aString.Append(PRUnichar('*'));
    }
    if (1 < aNegatedIndex) {
      NS_IF_NEGATED_END(aIsNegated, aString)
    }
  } else {
    // Append the tag name, if there is one
    if (mTag) {
      if (isPseudoElement) {
        if (!mNext) {
          // Lone pseudo-element selector -- toss in a wildcard type selector
          aString.Append(PRUnichar('*'));
        }
        if (!nsCSSPseudoElements::IsCSS2PseudoElement(mTag)) {
          aString.Append(PRUnichar(':'));
        }
      }
      nsAutoString prefix;
      mTag->ToString(prefix);
      aString.Append(prefix);
      NS_IF_NEGATED_END(aIsNegated, aString)
    }
    // Append the id, if there is one
    if (mIDList) {
      nsAtomList* list = mIDList;
      while (list != nsnull) {
        list->mAtom->ToString(temp);
        NS_IF_NEGATED_START(aIsNegated, aString)
        aString.Append(PRUnichar('#'));
        aString.Append(temp);
        NS_IF_NEGATED_END(aIsNegated, aString)
        list = list->mNext;
      }
    }
    // Append each class in the linked list
    if (mClassList) {
      nsAtomList* list = mClassList;
      while (list != nsnull) {
        list->mAtom->ToString(temp);
        NS_IF_NEGATED_START(aIsNegated, aString)
        aString.Append(PRUnichar('.'));
        aString.Append(temp);
        NS_IF_NEGATED_END(aIsNegated, aString)
        list = list->mNext;
      }
    }
  }

  // Append each attribute selector in the linked list
  if (mAttrList) {
    nsAttrSelector* list = mAttrList;
    while (list != nsnull) {
      NS_IF_NEGATED_START(aIsNegated, aString)
      aString.Append(PRUnichar('['));
      // Append the namespace prefix
      if (list->mNameSpace > 0) {
        nsCOMPtr<nsINameSpace> sheetNS;
        aSheet->GetNameSpace(*getter_AddRefs(sheetNS));
        nsCOMPtr<nsIAtom> prefixAtom;
        // will return null if namespace was the default
        sheetNS->FindNameSpacePrefix(list->mNameSpace,
                                     getter_AddRefs(prefixAtom));
        if (prefixAtom) { 
          nsAutoString prefix;
          prefixAtom->ToString(prefix);
          aString.Append(prefix);
          aString.Append(PRUnichar('|'));
        }
      }
      // Append the attribute name
      list->mAttr->ToString(temp);
      aString.Append(temp);
      // Append the function
      if (list->mFunction == NS_ATTR_FUNC_EQUALS) {
        aString.Append(PRUnichar('='));
      } else if (list->mFunction == NS_ATTR_FUNC_INCLUDES) {
        aString.Append(PRUnichar('~'));
        aString.Append(PRUnichar('='));
      } else if (list->mFunction == NS_ATTR_FUNC_DASHMATCH) {
        aString.Append(PRUnichar('|'));
        aString.Append(PRUnichar('='));
      } else if (list->mFunction == NS_ATTR_FUNC_BEGINSMATCH) {
        aString.Append(PRUnichar('^'));
        aString.Append(PRUnichar('='));
      } else if (list->mFunction == NS_ATTR_FUNC_ENDSMATCH) {
        aString.Append(PRUnichar('$'));
        aString.Append(PRUnichar('='));
      } else if (list->mFunction == NS_ATTR_FUNC_CONTAINSMATCH) {
        aString.Append(PRUnichar('*'));
        aString.Append(PRUnichar('='));
      }
      // Append the value
      aString.Append(list->mValue);
      aString.Append(PRUnichar(']'));
      NS_IF_NEGATED_END(aIsNegated, aString)
      list = list->mNext;
    }
  }

  // Append each pseudo-class in the linked list
  if (mPseudoClassList) {
    nsAtomStringList* list = mPseudoClassList;
    while (list != nsnull) {
      list->mAtom->ToString(temp);
      NS_IF_NEGATED_START(aIsNegated, aString)
      aString.Append(temp);
      if (nsnull != list->mString) {
        aString.Append(PRUnichar('('));
        aString.Append(list->mString);
        aString.Append(PRUnichar(')'));
      }
      NS_IF_NEGATED_END(aIsNegated, aString)
      list = list->mNext;
    }
  }

  if (mNegations) {
    // chain all the negated selectors
    mNegations->ToStringInternal(aString, aSheet, PR_FALSE, aNegatedIndex + 1);
  }

  // Append the operator only if the selector is not negated and is not
  // a pseudo-element
  if (!aIsNegated && mOperator && !aIsPseudoElem) {
    aString.Append(PRUnichar(' '));
    aString.Append(mOperator);
  }
}

// -- nsCSSSelectorList -------------------------------

MOZ_DECL_CTOR_COUNTER(nsCSSSelectorList)

nsCSSSelectorList::nsCSSSelectorList(void)
  : mSelectors(nsnull),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSSelectorList);
}

nsCSSSelectorList::~nsCSSSelectorList()
{
  MOZ_COUNT_DTOR(nsCSSSelectorList);
  nsCSSSelector*  sel = mSelectors;
  while (sel) {
    nsCSSSelector* dead = sel;
    sel = sel->mNext;
    delete dead;
  }
  if (mNext) {
    delete mNext;
  }
}

void nsCSSSelectorList::AddSelector(const nsCSSSelector& aSelector)
{ // prepend to list
  nsCSSSelector* newSel = new nsCSSSelector(aSelector);
  if (newSel) {
    newSel->mNext = mSelectors;
    mSelectors = newSel;
  }
}

void
nsCSSSelectorList::ToString(nsAString& aResult, nsICSSStyleSheet* aSheet)
{
  nsCSSSelectorList *p = this;
  for (;;) {
    p->mSelectors->ToString(aResult, aSheet);
    p = p->mNext;
    if (!p)
      break;
    aResult.Append(NS_LITERAL_STRING(", "));
  }
}

nsCSSSelectorList*
nsCSSSelectorList::Clone()
{
  nsCSSSelectorList *list = nsnull;
  nsCSSSelectorList **list_cur = &list;
  for (nsCSSSelectorList *l = this; l; l = l->mNext) {
    nsCSSSelectorList *lcopy = new nsCSSSelectorList();
    if (!lcopy) {
      delete list;
      return nsnull;
    }
    *list_cur = lcopy;
    list_cur = &lcopy->mNext;

    nsCSSSelector **sel_cur = &lcopy->mSelectors;
    for (nsCSSSelector *s = l->mSelectors; s; s = s->mNext) {
      nsCSSSelector *scopy = new nsCSSSelector(*s);
      if (!scopy) {
        delete list;
        return nsnull;
      }
      *sel_cur = scopy;
      sel_cur = &scopy->mNext;
    }
  }
  return list;
}

// -- CSSImportantRule -------------------------------

class CSSStyleRuleImpl;

class CSSImportantRule : public nsIStyleRule {
public:
  CSSImportantRule(nsICSSStyleSheet* aSheet, nsCSSDeclaration* aDeclaration);

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;

  // The new mapping function.
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

protected:
  virtual ~CSSImportantRule(void);

  nsCSSDeclaration*  mDeclaration;
  nsICSSStyleSheet*  mSheet;

friend class CSSStyleRuleImpl;
};

CSSImportantRule::CSSImportantRule(nsICSSStyleSheet* aSheet, nsCSSDeclaration* aDeclaration)
  : mDeclaration(aDeclaration),
    mSheet(aSheet)
{
}

CSSImportantRule::~CSSImportantRule(void)
{
  mDeclaration = nsnull;
}

NS_IMPL_ISUPPORTS1(CSSImportantRule, nsIStyleRule)

NS_IMETHODIMP
CSSImportantRule::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  NS_IF_ADDREF(mSheet);
  aSheet = mSheet;
  return NS_OK;
}

NS_IMETHODIMP
CSSImportantRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
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
  virtual nsresult GetCSSParsingEnvironment(nsIURI** aURI,
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

MOZ_DECL_CTOR_COUNTER(DOMCSSDeclarationImpl)

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
  return NS_REINTERPRET_CAST(DOMCSSStyleRuleImpl*,
           NS_REINTERPRET_CAST(char*, this) -
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
DOMCSSDeclarationImpl::GetCSSParsingEnvironment(nsIURI** aURI,
                                                nsICSSLoader** aCSSLoader,
                                                nsICSSParser** aCSSParser)
{
  // null out the out params since some of them may not get initialized below
  *aURI = nsnull;
  *aCSSLoader = nsnull;
  *aCSSParser = nsnull;
  nsresult result;
  nsCOMPtr<nsIStyleSheet> sheet;
  if (mRule) {
    mRule->GetStyleSheet(*getter_AddRefs(sheet));
    if (sheet) {
      sheet->GetURL(*aURI);
      nsCOMPtr<nsIDocument> document;
      sheet->GetOwningDocument(*getter_AddRefs(document));
      if (document) {
        NS_IF_ADDREF(*aCSSLoader = document->GetCSSLoader());
        NS_ASSERTION(*aCSSLoader, "Document with no CSS loader!");
      }
    }
  }
  // XXXldb Why bother if |mRule| is null?
  if (*aCSSLoader) {
    result = (*aCSSLoader)->GetParserFor(nsnull, aCSSParser);
  } else {
    result = NS_NewCSSParser(aCSSParser);
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

  NS_IMETHOD GetDOMRule(nsIDOMCSSRule** aDOMRule);

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
    mDOMRule(nsnull)
{
  mDeclaration->AddRef();
}

// for |Clone|
CSSStyleRuleImpl::CSSStyleRuleImpl(const CSSStyleRuleImpl& aCopy)
  : nsCSSRule(aCopy),
    mSelector(aCopy.mSelector ? aCopy.mSelector->Clone() : nsnull),
    mDeclaration(aCopy.mDeclaration->Clone()),
    mImportantRule(nsnull),
    mDOMRule(nsnull)
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
    mDOMRule(aCopy.mDOMRule)
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
    mImportantRule->mSheet = nsnull;
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
  if (!mImportantRule && mDeclaration->HasImportantData()) {
    mImportantRule = new CSSImportantRule(mSheet, mDeclaration);
    NS_IF_ADDREF(mImportantRule);
  }
  NS_IF_ADDREF(mImportantRule);
  return mImportantRule;
}

NS_IMETHODIMP
CSSStyleRuleImpl::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  return nsCSSRule::GetStyleSheet(aSheet);
}

NS_IMETHODIMP
CSSStyleRuleImpl::SetStyleSheet(nsICSSStyleSheet* aSheet)
{
  nsCSSRule::SetStyleSheet(aSheet);
  if (nsnull != mImportantRule) { // we're responsible for this guy too
    mImportantRule->mSheet = aSheet;
  }
  return NS_OK;
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

NS_IMETHODIMP
CSSStyleRuleImpl::GetDOMRule(nsIDOMCSSRule** aDOMRule)
{
  if (!mSheet) {
    // inline style rules aren't supposed to have a DOM rule object, only
    // a declaration.
    *aDOMRule = nsnull;
    return NS_OK;
  }
  if (!mDOMRule) {
    mDOMRule = new DOMCSSStyleRuleImpl(this);
    if (!mDOMRule) {
      *aDOMRule = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mDOMRule);
  }
  *aDOMRule = mDOMRule;
  NS_ADDREF(*aDOMRule);
  return NS_OK;
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

  buffer.Append(NS_LITERAL_STRING(" "));
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
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
