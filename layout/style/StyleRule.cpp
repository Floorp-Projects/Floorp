/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * representation of CSS style rules (selectors+declaration), CSS
 * selectors, and DOM objects for style rules, selectors, and
 * declarations
 */

#include "mozilla/css/StyleRule.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/css/GroupRule.h"
#include "mozilla/css/Declaration.h"
#include "nsCSSStyleSheet.h"
#include "nsIDocument.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "nsStyleUtil.h"
#include "nsICSSStyleRuleDOMWrapper.h"
#include "nsDOMCSSDeclaration.h"
#include "nsINameSpaceManager.h"
#include "nsXMLNameSpaceMap.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSPseudoClasses.h"
#include "nsCSSAnonBoxes.h"
#include "nsTArray.h"
#include "nsDOMClassInfoID.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "mozAutoDocUpdate.h"

class nsIDOMCSSStyleDeclaration;
class nsIDOMCSSStyleSheet;

using namespace mozilla;

#define NS_IF_CLONE(member_)                                                  \
  PR_BEGIN_MACRO                                                              \
    if (member_) {                                                            \
      result->member_ = member_->Clone();                                     \
      if (!result->member_) {                                                 \
        delete result;                                                        \
        return nullptr;                                                        \
      }                                                                       \
    }                                                                         \
  PR_END_MACRO

#define NS_IF_DELETE(ptr)                                                     \
  PR_BEGIN_MACRO                                                              \
    delete ptr;                                                               \
    ptr = nullptr;                                                             \
  PR_END_MACRO

/* ************************************************************************** */

nsAtomList::nsAtomList(nsIAtom* aAtom)
  : mAtom(aAtom),
    mNext(nullptr)
{
  MOZ_COUNT_CTOR(nsAtomList);
}

nsAtomList::nsAtomList(const nsString& aAtomValue)
  : mAtom(nullptr),
    mNext(nullptr)
{
  MOZ_COUNT_CTOR(nsAtomList);
  mAtom = do_GetAtom(aAtomValue);
}

nsAtomList*
nsAtomList::Clone(bool aDeep) const
{
  nsAtomList *result = new nsAtomList(mAtom);
  if (!result)
    return nullptr;

  if (aDeep)
    NS_CSS_CLONE_LIST_MEMBER(nsAtomList, this, mNext, result, (false));
  return result;
}

size_t
nsAtomList::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  const nsAtomList* a = this;
  while (a) {
    n += aMallocSizeOf(a);

    // The following members aren't measured:
    // - a->mAtom, because it may be shared

    a = a->mNext;
  }
  return n;
}

nsAtomList::~nsAtomList(void)
{
  MOZ_COUNT_DTOR(nsAtomList);
  NS_CSS_DELETE_LIST_MEMBER(nsAtomList, this, mNext);
}

nsPseudoClassList::nsPseudoClassList(nsCSSPseudoClasses::Type aType)
  : mType(aType),
    mNext(nullptr)
{
  NS_ASSERTION(!nsCSSPseudoClasses::HasStringArg(aType) &&
               !nsCSSPseudoClasses::HasNthPairArg(aType),
               "unexpected pseudo-class");
  MOZ_COUNT_CTOR(nsPseudoClassList);
  u.mMemory = nullptr;
}

nsPseudoClassList::nsPseudoClassList(nsCSSPseudoClasses::Type aType,
                                     const PRUnichar* aString)
  : mType(aType),
    mNext(nullptr)
{
  NS_ASSERTION(nsCSSPseudoClasses::HasStringArg(aType),
               "unexpected pseudo-class");
  NS_ASSERTION(aString, "string expected");
  MOZ_COUNT_CTOR(nsPseudoClassList);
  u.mString = NS_strdup(aString);
}

nsPseudoClassList::nsPseudoClassList(nsCSSPseudoClasses::Type aType,
                                     const int32_t* aIntPair)
  : mType(aType),
    mNext(nullptr)
{
  NS_ASSERTION(nsCSSPseudoClasses::HasNthPairArg(aType),
               "unexpected pseudo-class");
  NS_ASSERTION(aIntPair, "integer pair expected");
  MOZ_COUNT_CTOR(nsPseudoClassList);
  u.mNumbers =
    static_cast<int32_t*>(nsMemory::Clone(aIntPair, sizeof(int32_t) * 2));
}

// adopts aSelectorList
nsPseudoClassList::nsPseudoClassList(nsCSSPseudoClasses::Type aType,
                                     nsCSSSelectorList* aSelectorList)
  : mType(aType),
    mNext(nullptr)
{
  NS_ASSERTION(nsCSSPseudoClasses::HasSelectorListArg(aType),
               "unexpected pseudo-class");
  NS_ASSERTION(aSelectorList, "selector list expected");
  MOZ_COUNT_CTOR(nsPseudoClassList);
  u.mSelectors = aSelectorList;
}

nsPseudoClassList*
nsPseudoClassList::Clone(bool aDeep) const
{
  nsPseudoClassList *result;
  if (!u.mMemory) {
    result = new nsPseudoClassList(mType);
  } else if (nsCSSPseudoClasses::HasStringArg(mType)) {
    result = new nsPseudoClassList(mType, u.mString);
  } else if (nsCSSPseudoClasses::HasNthPairArg(mType)) {
    result = new nsPseudoClassList(mType, u.mNumbers);
  } else {
    NS_ASSERTION(nsCSSPseudoClasses::HasSelectorListArg(mType),
                 "unexpected pseudo-class");
    // This constructor adopts its selector list argument.
    result = new nsPseudoClassList(mType, u.mSelectors->Clone());
  }

  if (aDeep)
    NS_CSS_CLONE_LIST_MEMBER(nsPseudoClassList, this, mNext, result,
                             (false));

  return result;
}

size_t
nsPseudoClassList::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  const nsPseudoClassList* p = this;
  while (p) {
    n += aMallocSizeOf(p);
    if (!p->u.mMemory) {
      // do nothing

    } else if (nsCSSPseudoClasses::HasStringArg(p->mType)) {
      n += aMallocSizeOf(p->u.mString);

    } else if (nsCSSPseudoClasses::HasNthPairArg(p->mType)) {
      n += aMallocSizeOf(p->u.mNumbers);

    } else {
      NS_ASSERTION(nsCSSPseudoClasses::HasSelectorListArg(p->mType),
                   "unexpected pseudo-class");
      n += p->u.mSelectors->SizeOfIncludingThis(aMallocSizeOf);
    }
    p = p->mNext;
  }
  return n;
}

nsPseudoClassList::~nsPseudoClassList(void)
{
  MOZ_COUNT_DTOR(nsPseudoClassList);
  if (nsCSSPseudoClasses::HasSelectorListArg(mType)) {
    delete u.mSelectors;
  } else if (u.mMemory) {
    NS_Free(u.mMemory);
  }
  NS_CSS_DELETE_LIST_MEMBER(nsPseudoClassList, this, mNext);
}

nsAttrSelector::nsAttrSelector(int32_t aNameSpace, const nsString& aAttr)
  : mValue(),
    mNext(nullptr),
    mLowercaseAttr(nullptr),
    mCasedAttr(nullptr),
    mNameSpace(aNameSpace),
    mFunction(NS_ATTR_FUNC_SET),
    mCaseSensitive(1)
{
  MOZ_COUNT_CTOR(nsAttrSelector);

  nsAutoString lowercase;
  nsContentUtils::ASCIIToLower(aAttr, lowercase);
  
  mCasedAttr = do_GetAtom(aAttr);
  mLowercaseAttr = do_GetAtom(lowercase);
}

nsAttrSelector::nsAttrSelector(int32_t aNameSpace, const nsString& aAttr, uint8_t aFunction, 
                               const nsString& aValue, bool aCaseSensitive)
  : mValue(aValue),
    mNext(nullptr),
    mLowercaseAttr(nullptr),
    mCasedAttr(nullptr),
    mNameSpace(aNameSpace),
    mFunction(aFunction),
    mCaseSensitive(aCaseSensitive)
{
  MOZ_COUNT_CTOR(nsAttrSelector);

  nsAutoString lowercase;
  nsContentUtils::ASCIIToLower(aAttr, lowercase);
  
  mCasedAttr = do_GetAtom(aAttr);
  mLowercaseAttr = do_GetAtom(lowercase);
}

nsAttrSelector::nsAttrSelector(int32_t aNameSpace,  nsIAtom* aLowercaseAttr,
                               nsIAtom* aCasedAttr, uint8_t aFunction, 
                               const nsString& aValue, bool aCaseSensitive)
  : mValue(aValue),
    mNext(nullptr),
    mLowercaseAttr(aLowercaseAttr),
    mCasedAttr(aCasedAttr),
    mNameSpace(aNameSpace),
    mFunction(aFunction),
    mCaseSensitive(aCaseSensitive)
{
  MOZ_COUNT_CTOR(nsAttrSelector);
}

nsAttrSelector*
nsAttrSelector::Clone(bool aDeep) const
{
  nsAttrSelector *result =
    new nsAttrSelector(mNameSpace, mLowercaseAttr, mCasedAttr, 
                       mFunction, mValue, mCaseSensitive);

  if (aDeep)
    NS_CSS_CLONE_LIST_MEMBER(nsAttrSelector, this, mNext, result, (false));

  return result;
}

nsAttrSelector::~nsAttrSelector(void)
{
  MOZ_COUNT_DTOR(nsAttrSelector);

  NS_CSS_DELETE_LIST_MEMBER(nsAttrSelector, this, mNext);
}

// -- nsCSSSelector -------------------------------

nsCSSSelector::nsCSSSelector(void)
  : mLowercaseTag(nullptr),
    mCasedTag(nullptr),
    mIDList(nullptr),
    mClassList(nullptr),
    mPseudoClassList(nullptr),
    mAttrList(nullptr),
    mNegations(nullptr),
    mNext(nullptr),
    mNameSpace(kNameSpaceID_Unknown),
    mOperator(0),
    mPseudoType(nsCSSPseudoElements::ePseudo_NotPseudoElement)
{
  MOZ_COUNT_CTOR(nsCSSSelector);
  static_assert(nsCSSPseudoElements::ePseudo_MAX < INT16_MAX,
                "nsCSSPseudoElements::Type values overflow mPseudoType");
}

nsCSSSelector*
nsCSSSelector::Clone(bool aDeepNext, bool aDeepNegations) const
{
  nsCSSSelector *result = new nsCSSSelector();
  if (!result)
    return nullptr;

  result->mNameSpace = mNameSpace;
  result->mLowercaseTag = mLowercaseTag;
  result->mCasedTag = mCasedTag;
  result->mOperator = mOperator;
  result->mPseudoType = mPseudoType;
  
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
                             (true, false));
  }

  if (aDeepNext) {
    NS_CSS_CLONE_LIST_MEMBER(nsCSSSelector, this, mNext, result,
                             (false, true));
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
  mLowercaseTag = nullptr;
  mCasedTag = nullptr;
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

void nsCSSSelector::SetNameSpace(int32_t aNameSpace)
{
  mNameSpace = aNameSpace;
}

void nsCSSSelector::SetTag(const nsString& aTag)
{
  if (aTag.IsEmpty()) {
    mLowercaseTag = mCasedTag =  nullptr;
    return;
  }

  mCasedTag = do_GetAtom(aTag);
 
  nsAutoString lowercase;
  nsContentUtils::ASCIIToLower(aTag, lowercase);
  mLowercaseTag = do_GetAtom(lowercase);
}

void nsCSSSelector::AddID(const nsString& aID)
{
  if (!aID.IsEmpty()) {
    nsAtomList** list = &mIDList;
    while (nullptr != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAtomList(aID);
  }
}

void nsCSSSelector::AddClass(const nsString& aClass)
{
  if (!aClass.IsEmpty()) {
    nsAtomList** list = &mClassList;
    while (nullptr != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAtomList(aClass);
  }
}

void nsCSSSelector::AddPseudoClass(nsCSSPseudoClasses::Type aType)
{
  AddPseudoClassInternal(new nsPseudoClassList(aType));
}

void nsCSSSelector::AddPseudoClass(nsCSSPseudoClasses::Type aType,
                                   const PRUnichar* aString)
{
  AddPseudoClassInternal(new nsPseudoClassList(aType, aString));
}

void nsCSSSelector::AddPseudoClass(nsCSSPseudoClasses::Type aType,
                                   const int32_t* aIntPair)
{
  AddPseudoClassInternal(new nsPseudoClassList(aType, aIntPair));
}

void nsCSSSelector::AddPseudoClass(nsCSSPseudoClasses::Type aType,
                                   nsCSSSelectorList* aSelectorList)
{
  // Take ownership of nsCSSSelectorList instead of copying.
  AddPseudoClassInternal(new nsPseudoClassList(aType, aSelectorList));
}

void nsCSSSelector::AddPseudoClassInternal(nsPseudoClassList *aPseudoClass)
{
  nsPseudoClassList** list = &mPseudoClassList;
  while (nullptr != *list) {
    list = &((*list)->mNext);
  }
  *list = aPseudoClass;
}

void nsCSSSelector::AddAttribute(int32_t aNameSpace, const nsString& aAttr)
{
  if (!aAttr.IsEmpty()) {
    nsAttrSelector** list = &mAttrList;
    while (nullptr != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAttrSelector(aNameSpace, aAttr);
  }
}

void nsCSSSelector::AddAttribute(int32_t aNameSpace, const nsString& aAttr, uint8_t aFunc, 
                                 const nsString& aValue, bool aCaseSensitive)
{
  if (!aAttr.IsEmpty()) {
    nsAttrSelector** list = &mAttrList;
    while (nullptr != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAttrSelector(aNameSpace, aAttr, aFunc, aValue, aCaseSensitive);
  }
}

void nsCSSSelector::SetOperator(PRUnichar aOperator)
{
  mOperator = aOperator;
}

int32_t nsCSSSelector::CalcWeightWithoutNegations() const
{
  int32_t weight = 0;

  if (nullptr != mLowercaseTag) {
    weight += 0x000001;
  }
  nsAtomList* list = mIDList;
  while (nullptr != list) {
    weight += 0x010000;
    list = list->mNext;
  }
  list = mClassList;
  while (nullptr != list) {
    weight += 0x000100;
    list = list->mNext;
  }
  // FIXME (bug 561154):  This is incorrect for :-moz-any(), which isn't
  // really a pseudo-class.  In order to handle :-moz-any() correctly,
  // we need to compute specificity after we match, based on which
  // option we matched with (and thus also need to try the
  // highest-specificity options first).
  nsPseudoClassList *plist = mPseudoClassList;
  while (nullptr != plist) {
    weight += 0x000100;
    plist = plist->mNext;
  }
  nsAttrSelector* attr = mAttrList;
  while (nullptr != attr) {
    weight += 0x000100;
    attr = attr->mNext;
  }
  return weight;
}

int32_t nsCSSSelector::CalcWeight() const
{
  // Loop over this selector and all its negations.
  int32_t weight = 0;
  for (const nsCSSSelector *n = this; n; n = n->mNegations) {
    weight += n->CalcWeightWithoutNegations();
  }
  return weight;
}

//
// Builds the textual representation of a selector. Called by DOM 2 CSS 
// StyleRule:selectorText
//
void
nsCSSSelector::ToString(nsAString& aString, nsCSSStyleSheet* aSheet,
                        bool aAppend) const
{
  if (!aAppend)
   aString.Truncate();

  // selectors are linked from right-to-left, so the next selector in
  // the linked list actually precedes this one in the resulting string
  nsAutoTArray<const nsCSSSelector*, 8> stack;
  for (const nsCSSSelector *s = this; s; s = s->mNext) {
    stack.AppendElement(s);
  }

  while (!stack.IsEmpty()) {
    uint32_t index = stack.Length() - 1;
    const nsCSSSelector *s = stack.ElementAt(index);
    stack.RemoveElementAt(index);

    s->AppendToStringWithoutCombinators(aString, aSheet);

    // Append the combinator, if needed.
    if (!stack.IsEmpty()) {
      const nsCSSSelector *next = stack.ElementAt(index - 1);
      PRUnichar oper = s->mOperator;
      if (next->IsPseudoElement()) {
        NS_ASSERTION(oper == PRUnichar('>'),
                     "improperly chained pseudo element");
      } else {
        NS_ASSERTION(oper != PRUnichar(0),
                     "compound selector without combinator");

        aString.Append(PRUnichar(' '));
        if (oper != PRUnichar(' ')) {
          aString.Append(oper);
          aString.Append(PRUnichar(' '));
        }
      }
    }
  }
}

void
nsCSSSelector::AppendToStringWithoutCombinators
                   (nsAString& aString, nsCSSStyleSheet* aSheet) const
{
  AppendToStringWithoutCombinatorsOrNegations(aString, aSheet, false);

  for (const nsCSSSelector* negation = mNegations; negation;
       negation = negation->mNegations) {
    aString.AppendLiteral(":not(");
    negation->AppendToStringWithoutCombinatorsOrNegations(aString, aSheet,
                                                          true);
    aString.Append(PRUnichar(')'));
  }
}

void
nsCSSSelector::AppendToStringWithoutCombinatorsOrNegations
                   (nsAString& aString, nsCSSStyleSheet* aSheet,
                   bool aIsNegated) const
{
  nsAutoString temp;
  bool isPseudoElement = IsPseudoElement();

  // For non-pseudo-element selectors or for lone pseudo-elements, deal with
  // namespace prefixes.
  bool wroteNamespace = false;
  if (!isPseudoElement || !mNext) {
    // append the namespace prefix if needed
    nsXMLNameSpaceMap *sheetNS = aSheet ? aSheet->GetNameSpaceMap() : nullptr;

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
        wroteNamespace = true;
      }
    } else if (sheetNS->FindNameSpaceID(nullptr) == mNameSpace) {
      // We have the default namespace (possibly including the wildcard
      // namespace).  Do nothing.
      NS_ASSERTION(mNameSpace == kNameSpaceID_Unknown ||
                   CanBeNamespaced(aIsNegated),
                   "How did we end up with this namespace?");
    } else if (mNameSpace == kNameSpaceID_None) {
      NS_ASSERTION(CanBeNamespaced(aIsNegated),
                   "How did we end up with this namespace?");
      aString.Append(PRUnichar('|'));
      wroteNamespace = true;
    } else if (mNameSpace != kNameSpaceID_Unknown) {
      NS_ASSERTION(CanBeNamespaced(aIsNegated),
                   "How did we end up with this namespace?");
      nsIAtom *prefixAtom = sheetNS->FindPrefix(mNameSpace);
      NS_ASSERTION(prefixAtom, "how'd we get a non-default namespace "
                   "without a prefix?");
      nsStyleUtil::AppendEscapedCSSIdent(nsDependentAtomString(prefixAtom),
                                         aString);
      aString.Append(PRUnichar('|'));
      wroteNamespace = true;
    } else {
      // A selector for an element in any namespace, while the default
      // namespace is something else.  :not() is special in that the default
      // namespace is not implied for non-type selectors, so if this is a
      // negated non-type selector we don't need to output an explicit wildcard
      // namespace here, since those default to a wildcard namespace.
      if (CanBeNamespaced(aIsNegated)) {
        aString.AppendLiteral("*|");
        wroteNamespace = true;
      }
    }
  }
      
  if (!mLowercaseTag) {
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
    nsAutoString tag;
    (isPseudoElement ? mLowercaseTag : mCasedTag)->ToString(tag);
    if (isPseudoElement) {
      if (!mNext) {
        // Lone pseudo-element selector -- toss in a wildcard type selector
        // XXXldb Why?
        aString.Append(PRUnichar('*'));
      }
      if (!nsCSSPseudoElements::IsCSS2PseudoElement(mLowercaseTag)) {
        aString.Append(PRUnichar(':'));
      }
      // This should not be escaped since (a) the pseudo-element string
      // has a ":" that can't be escaped and (b) all pseudo-elements at
      // this point are known, and therefore we know they don't need
      // escaping.
      aString.Append(tag);
    } else {
      nsStyleUtil::AppendEscapedCSSIdent(tag, aString);
    }
  }

  // Append the id, if there is one
  if (mIDList) {
    nsAtomList* list = mIDList;
    while (list != nullptr) {
      list->mAtom->ToString(temp);
      aString.Append(PRUnichar('#'));
      nsStyleUtil::AppendEscapedCSSIdent(temp, aString);
      list = list->mNext;
    }
  }

  // Append each class in the linked list
  if (mClassList) {
    if (isPseudoElement) {
#ifdef MOZ_XUL
      NS_ABORT_IF_FALSE(nsCSSAnonBoxes::IsTreePseudoElement(mLowercaseTag),
                        "must be tree pseudo-element");

      aString.Append(PRUnichar('('));
      for (nsAtomList* list = mClassList; list; list = list->mNext) {
        nsStyleUtil::AppendEscapedCSSIdent(nsDependentAtomString(list->mAtom), aString);
        aString.Append(PRUnichar(','));
      }
      // replace the final comma with a close-paren
      aString.Replace(aString.Length() - 1, 1, PRUnichar(')'));
#else
      NS_ERROR("Can't happen");
#endif
    } else {
      nsAtomList* list = mClassList;
      while (list != nullptr) {
        list->mAtom->ToString(temp);
        aString.Append(PRUnichar('.'));
        nsStyleUtil::AppendEscapedCSSIdent(temp, aString);
        list = list->mNext;
      }
    }
  }

  // Append each attribute selector in the linked list
  if (mAttrList) {
    nsAttrSelector* list = mAttrList;
    while (list != nullptr) {
      aString.Append(PRUnichar('['));
      // Append the namespace prefix
      if (list->mNameSpace == kNameSpaceID_Unknown) {
        aString.Append(PRUnichar('*'));
        aString.Append(PRUnichar('|'));
      } else if (list->mNameSpace != kNameSpaceID_None) {
        if (aSheet) {
          nsXMLNameSpaceMap *sheetNS = aSheet->GetNameSpaceMap();
          nsIAtom *prefixAtom = sheetNS->FindPrefix(list->mNameSpace);
          // Default namespaces don't apply to attribute selectors, so
          // we must have a useful prefix.
          NS_ASSERTION(prefixAtom,
                       "How did we end up with a namespace if the prefix "
                       "is unknown?");
          nsAutoString prefix;
          prefixAtom->ToString(prefix);
          nsStyleUtil::AppendEscapedCSSIdent(prefix, aString);
          aString.Append(PRUnichar('|'));
        }
      }
      // Append the attribute name
      list->mCasedAttr->ToString(temp);
      nsStyleUtil::AppendEscapedCSSIdent(temp, aString);

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
        nsStyleUtil::AppendEscapedCSSString(list->mValue, aString);
      }

      aString.Append(PRUnichar(']'));
      
      list = list->mNext;
    }
  }

  // Append each pseudo-class in the linked list
  for (nsPseudoClassList* list = mPseudoClassList; list; list = list->mNext) {
    nsCSSPseudoClasses::PseudoTypeToString(list->mType, temp);
    // This should not be escaped since (a) the pseudo-class string
    // has a ":" that can't be escaped and (b) all pseudo-classes at
    // this point are known, and therefore we know they don't need
    // escaping.
    aString.Append(temp);
    if (list->u.mMemory) {
      aString.Append(PRUnichar('('));
      if (nsCSSPseudoClasses::HasStringArg(list->mType)) {
        nsStyleUtil::AppendEscapedCSSIdent(
          nsDependentString(list->u.mString), aString);
      } else if (nsCSSPseudoClasses::HasNthPairArg(list->mType)) {
        int32_t a = list->u.mNumbers[0],
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
      } else {
        NS_ASSERTION(nsCSSPseudoClasses::HasSelectorListArg(list->mType),
                     "unexpected pseudo-class");
        nsString tmp;
        list->u.mSelectors->ToString(tmp, aSheet);
        aString.Append(tmp);
      }
      aString.Append(PRUnichar(')'));
    }
  }
}

bool
nsCSSSelector::CanBeNamespaced(bool aIsNegated) const
{
  return !aIsNegated ||
         (!mIDList && !mClassList && !mPseudoClassList && !mAttrList);
}

size_t
nsCSSSelector::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  const nsCSSSelector* s = this;
  while (s) {
    n += aMallocSizeOf(s);

    #define MEASURE(x)   n += x ? x->SizeOfIncludingThis(aMallocSizeOf) : 0;

    MEASURE(s->mIDList);
    MEASURE(s->mClassList);
    MEASURE(s->mPseudoClassList);
    MEASURE(s->mNegations);

    // Measurement of the following members may be added later if DMD finds it is
    // worthwhile:
    // - s->mAttrList
    //
    // The following members aren't measured:
    // - s->mLowercaseTag, because it's an atom and therefore shared
    // - s->mCasedTag, because it's an atom and therefore shared

    s = s->mNext;
  }
  return n;
}

// -- nsCSSSelectorList -------------------------------

nsCSSSelectorList::nsCSSSelectorList(void)
  : mSelectors(nullptr),
    mWeight(0),
    mNext(nullptr)
{
  MOZ_COUNT_CTOR(nsCSSSelectorList);
}

nsCSSSelectorList::~nsCSSSelectorList()
{
  MOZ_COUNT_DTOR(nsCSSSelectorList);
  delete mSelectors;
  NS_CSS_DELETE_LIST_MEMBER(nsCSSSelectorList, this, mNext);
}

nsCSSSelector*
nsCSSSelectorList::AddSelector(PRUnichar aOperator)
{
  nsCSSSelector* newSel = new nsCSSSelector();

  if (mSelectors) {
    NS_ASSERTION(aOperator != PRUnichar(0), "chaining without combinator");
    mSelectors->SetOperator(aOperator);
  } else {
    NS_ASSERTION(aOperator == PRUnichar(0), "combinator without chaining");
  }

  newSel->mNext = mSelectors;
  mSelectors = newSel;
  return newSel;
}

void
nsCSSSelectorList::ToString(nsAString& aResult, nsCSSStyleSheet* aSheet)
{
  aResult.Truncate();
  nsCSSSelectorList *p = this;
  for (;;) {
    p->mSelectors->ToString(aResult, aSheet, true);
    p = p->mNext;
    if (!p)
      break;
    aResult.AppendLiteral(", ");
  }
}

nsCSSSelectorList*
nsCSSSelectorList::Clone(bool aDeep) const
{
  nsCSSSelectorList *result = new nsCSSSelectorList();
  result->mWeight = mWeight;
  NS_IF_CLONE(mSelectors);

  if (aDeep) {
    NS_CSS_CLONE_LIST_MEMBER(nsCSSSelectorList, this, mNext, result,
                             (false));
  }
  return result;
}

size_t
nsCSSSelectorList::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  const nsCSSSelectorList* s = this;
  while (s) {
    n += aMallocSizeOf(s);
    n += s->mSelectors ? s->mSelectors->SizeOfIncludingThis(aMallocSizeOf) : 0;
    s = s->mNext;
  }
  return n;
}

// -- ImportantRule ----------------------------------

namespace mozilla {
namespace css {

ImportantRule::ImportantRule(Declaration* aDeclaration)
  : mDeclaration(aDeclaration)
{
}

ImportantRule::~ImportantRule()
{
}

NS_IMPL_ISUPPORTS1(ImportantRule, nsIStyleRule)

/* virtual */ void
ImportantRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  mDeclaration->MapImportantRuleInfoInto(aRuleData);
}

#ifdef DEBUG
/* virtual */ void
ImportantRule::List(FILE* out, int32_t aIndent) const
{
  // Indent
  for (int32_t index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "! Important declaration=%p\n",
          static_cast<void*>(mDeclaration));
}
#endif

} // namespace css
} // namespace mozilla

// --------------------------------------------------------

namespace mozilla {
namespace css {
class DOMCSSStyleRule;
}
}

class DOMCSSDeclarationImpl : public nsDOMCSSDeclaration
{
public:
  DOMCSSDeclarationImpl(css::StyleRule *aRule);
  virtual ~DOMCSSDeclarationImpl(void);

  NS_IMETHOD GetParentRule(nsIDOMCSSRule **aParent);
  void DropReference(void);
  virtual css::Declaration* GetCSSDeclaration(bool aAllocate);
  virtual nsresult SetCSSDeclaration(css::Declaration* aDecl);
  virtual void GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv);
  virtual nsIDocument* DocToUpdate();

  // Override |AddRef| and |Release| for being a member of
  // |DOMCSSStyleRule|.  Also, we need to forward QI for cycle
  // collection things to DOMCSSStyleRule.
  NS_DECL_ISUPPORTS_INHERITED

  virtual nsINode *GetParentObject()
  {
    return mRule ? mRule->GetDocument() : nullptr;
  }

  friend class css::DOMCSSStyleRule;

protected:
  // This reference is not reference-counted. The rule object tells us
  // when it's about to go away.
  css::StyleRule *mRule;

  inline css::DOMCSSStyleRule* DomRule();

private:
  // NOT TO BE IMPLEMENTED
  // This object cannot be allocated on its own.  It must be a member of
  // DOMCSSStyleRule.
  void* operator new(size_t size) CPP_THROW_NEW;
};

namespace mozilla {
namespace css {

class DOMCSSStyleRule : public nsICSSStyleRuleDOMWrapper
{
public:
  DOMCSSStyleRule(StyleRule *aRule);
  virtual ~DOMCSSStyleRule();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMCSSStyleRule)
  NS_DECL_NSIDOMCSSRULE
  NS_DECL_NSIDOMCSSSTYLERULE

  // nsICSSStyleRuleDOMWrapper
  NS_IMETHOD GetCSSStyleRule(StyleRule **aResult);

  DOMCSSDeclarationImpl* DOMDeclaration() { return &mDOMDeclaration; }

  friend class ::DOMCSSDeclarationImpl;

protected:
  DOMCSSDeclarationImpl mDOMDeclaration;

  StyleRule* Rule() {
    return mDOMDeclaration.mRule;
  }
};

} // namespace css
} // namespace mozilla

DOMCSSDeclarationImpl::DOMCSSDeclarationImpl(css::StyleRule *aRule)
  : mRule(aRule)
{
  MOZ_COUNT_CTOR(DOMCSSDeclarationImpl);
}

DOMCSSDeclarationImpl::~DOMCSSDeclarationImpl(void)
{
  NS_ASSERTION(!mRule, "DropReference not called.");

  MOZ_COUNT_DTOR(DOMCSSDeclarationImpl);
}

inline css::DOMCSSStyleRule* DOMCSSDeclarationImpl::DomRule()
{
  return reinterpret_cast<css::DOMCSSStyleRule*>
                         (reinterpret_cast<char*>(this) -
           offsetof(css::DOMCSSStyleRule, mDOMDeclaration));
}

NS_IMPL_ADDREF_USING_AGGREGATOR(DOMCSSDeclarationImpl, DomRule())
NS_IMPL_RELEASE_USING_AGGREGATOR(DOMCSSDeclarationImpl, DomRule())

NS_INTERFACE_MAP_BEGIN(DOMCSSDeclarationImpl)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  // We forward the cycle collection interfaces to DomRule(), which is
  // never null (in fact, we're part of that object!)
  if (aIID.Equals(NS_GET_IID(nsCycleCollectionISupports)) ||
      aIID.Equals(NS_GET_IID(nsXPCOMCycleCollectionParticipant))) {
    return DomRule()->QueryInterface(aIID, aInstancePtr);
  }
  else
NS_IMPL_QUERY_TAIL_INHERITING(nsDOMCSSDeclaration)

void
DOMCSSDeclarationImpl::DropReference(void)
{
  mRule = nullptr;
}

css::Declaration*
DOMCSSDeclarationImpl::GetCSSDeclaration(bool aAllocate)
{
  if (mRule) {
    return mRule->GetDeclaration();
  } else {
    return nullptr;
  }
}

void
DOMCSSDeclarationImpl::GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv)
{
  GetCSSParsingEnvironmentForRule(mRule, aCSSParseEnv);
}

NS_IMETHODIMP
DOMCSSDeclarationImpl::GetParentRule(nsIDOMCSSRule **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);

  if (!mRule) {
    *aParent = nullptr;
    return NS_OK;
  }

  NS_IF_ADDREF(*aParent = mRule->GetDOMRule());
  return NS_OK;
}

nsresult
DOMCSSDeclarationImpl::SetCSSDeclaration(css::Declaration* aDecl)
{
  NS_PRECONDITION(mRule,
         "can only be called when |GetCSSDeclaration| returned a declaration");

  nsCOMPtr<nsIDocument> owningDoc;
  nsCOMPtr<nsIStyleSheet> sheet = mRule->GetStyleSheet();
  if (sheet) {
    owningDoc = sheet->GetOwningDocument();
  }

  mozAutoDocUpdate updateBatch(owningDoc, UPDATE_STYLE, true);

  nsRefPtr<css::StyleRule> oldRule = mRule;
  mRule = oldRule->DeclarationChanged(aDecl, true).get();
  if (!mRule)
    return NS_ERROR_OUT_OF_MEMORY;
  nsrefcnt cnt = mRule->Release();
  if (cnt == 0) {
    NS_NOTREACHED("container didn't take ownership");
    mRule = nullptr;
    return NS_ERROR_UNEXPECTED;
  }

  if (owningDoc) {
    owningDoc->StyleRuleChanged(sheet, oldRule, mRule);
  }
  return NS_OK;
}

nsIDocument*
DOMCSSDeclarationImpl::DocToUpdate()
{
  return nullptr;
}

// needs to be outside the namespace
DOMCI_DATA(CSSStyleRule, css::DOMCSSStyleRule)

namespace mozilla {
namespace css {

DOMCSSStyleRule::DOMCSSStyleRule(StyleRule* aRule)
  : mDOMDeclaration(aRule)
{
}

DOMCSSStyleRule::~DOMCSSStyleRule()
{
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMCSSStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsICSSStyleRuleDOMWrapper)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSStyleRule)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMCSSStyleRule)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMCSSStyleRule)

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMCSSStyleRule)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(DOMCSSStyleRule)
  // Trace the wrapper for our declaration.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  tmp->DOMDeclaration()->TraceWrapper(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMCSSStyleRule)
  // Unlink the wrapper for our declaraton.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  tmp->DOMDeclaration()->ReleaseWrapper(static_cast<nsISupports*>(p));
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMCSSStyleRule)
  // Just NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS here: that will call
  // into our Trace hook, where we do the right thing with declarations
  // already.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMETHODIMP
DOMCSSStyleRule::GetType(uint16_t* aType)
{
  *aType = nsIDOMCSSRule::STYLE_RULE;
  
  return NS_OK;
}

NS_IMETHODIMP
DOMCSSStyleRule::GetCssText(nsAString& aCssText)
{
  if (!Rule()) {
    aCssText.Truncate();
  } else {
    Rule()->GetCssText(aCssText);
  }
  return NS_OK;
}

NS_IMETHODIMP
DOMCSSStyleRule::SetCssText(const nsAString& aCssText)
{
  if (Rule()) {
    Rule()->SetCssText(aCssText);
  }
  return NS_OK;
}

NS_IMETHODIMP
DOMCSSStyleRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  if (!Rule()) {
    *aSheet = nullptr;
    return NS_OK;
  }
  return Rule()->GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
DOMCSSStyleRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  if (!Rule()) {
    *aParentRule = nullptr;
    return NS_OK;
  }
  return Rule()->GetParentRule(aParentRule);
}

NS_IMETHODIMP
DOMCSSStyleRule::GetSelectorText(nsAString& aSelectorText)
{
  if (!Rule()) {
    aSelectorText.Truncate();
  } else {
    Rule()->GetSelectorText(aSelectorText);
  }
  return NS_OK;
}

NS_IMETHODIMP
DOMCSSStyleRule::SetSelectorText(const nsAString& aSelectorText)
{
  if (Rule()) {
    Rule()->SetSelectorText(aSelectorText);
  }
  return NS_OK;
}

NS_IMETHODIMP
DOMCSSStyleRule::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  *aStyle = &mDOMDeclaration;
  NS_ADDREF(*aStyle);
  return NS_OK;
}

NS_IMETHODIMP
DOMCSSStyleRule::GetCSSStyleRule(StyleRule **aResult)
{
  *aResult = Rule();
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

} // namespace css
} // namespace mozilla

// -- StyleRule ------------------------------------

namespace mozilla {
namespace css {

StyleRule::StyleRule(nsCSSSelectorList* aSelector,
                     Declaration* aDeclaration)
  : Rule(),
    mSelector(aSelector),
    mDeclaration(aDeclaration),
    mImportantRule(nullptr),
    mDOMRule(nullptr),
    mLineNumber(0),
    mColumnNumber(0),
    mWasMatched(false)
{
  NS_PRECONDITION(aDeclaration, "must have a declaration");
}

// for |Clone|
StyleRule::StyleRule(const StyleRule& aCopy)
  : Rule(aCopy),
    mSelector(aCopy.mSelector ? aCopy.mSelector->Clone() : nullptr),
    mDeclaration(new Declaration(*aCopy.mDeclaration)),
    mImportantRule(nullptr),
    mDOMRule(nullptr),
    mLineNumber(aCopy.mLineNumber),
    mColumnNumber(aCopy.mColumnNumber),
    mWasMatched(false)
{
  // rest is constructed lazily on existing data
}

// for |SetCSSDeclaration|
StyleRule::StyleRule(StyleRule& aCopy,
                     Declaration* aDeclaration)
  : Rule(aCopy),
    mSelector(aCopy.mSelector),
    mDeclaration(aDeclaration),
    mImportantRule(nullptr),
    mDOMRule(aCopy.mDOMRule),
    mLineNumber(aCopy.mLineNumber),
    mColumnNumber(aCopy.mColumnNumber),
    mWasMatched(false)
{
  // The DOM rule is replacing |aCopy| with |this|, so transfer
  // the reverse pointer as well (and transfer ownership).
  aCopy.mDOMRule = nullptr;

  // Similarly for the selector.
  aCopy.mSelector = nullptr;

  // We are probably replacing the old declaration with |aDeclaration|
  // instead of taking ownership of the old declaration; only null out
  // aCopy.mDeclaration if we are taking ownership.
  if (mDeclaration == aCopy.mDeclaration) {
    // This should only ever happen if the declaration was modifiable.
    mDeclaration->AssertMutable();
    aCopy.mDeclaration = nullptr;
  }
}

StyleRule::~StyleRule()
{
  delete mSelector;
  delete mDeclaration;
  NS_IF_RELEASE(mImportantRule);
  if (mDOMRule) {
    mDOMRule->DOMDeclaration()->DropReference();
    NS_RELEASE(mDOMRule);
  }
}

// QueryInterface implementation for StyleRule
NS_INTERFACE_MAP_BEGIN(StyleRule)
  if (aIID.Equals(NS_GET_IID(mozilla::css::StyleRule))) {
    *aInstancePtr = this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStyleRule)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(StyleRule)
NS_IMPL_RELEASE(StyleRule)

void
StyleRule::RuleMatched()
{
  if (!mWasMatched) {
    NS_ABORT_IF_FALSE(!mImportantRule, "should not have important rule yet");

    mWasMatched = true;
    mDeclaration->SetImmutable();
    if (mDeclaration->HasImportantData()) {
      NS_ADDREF(mImportantRule = new ImportantRule(mDeclaration));
    }
  }
}

/* virtual */ int32_t
StyleRule::GetType() const
{
  return Rule::STYLE_RULE;
}

/* virtual */ already_AddRefed<Rule>
StyleRule::Clone() const
{
  nsRefPtr<Rule> clone = new StyleRule(*this);
  return clone.forget();
}

/* virtual */ nsIDOMCSSRule*
StyleRule::GetDOMRule()
{
  if (!mDOMRule) {
    if (!GetStyleSheet()) {
      // Inline style rules aren't supposed to have a DOM rule object, only
      // a declaration.  But if we do have one already, from a style sheet
      // rule that used to be in a document, we still want to return it.
      return nullptr;
    }
    mDOMRule = new DOMCSSStyleRule(this);
    NS_ADDREF(mDOMRule);
  }
  return mDOMRule;
}

/* virtual */ nsIDOMCSSRule*
StyleRule::GetExistingDOMRule()
{
  return mDOMRule;
}

/* virtual */ already_AddRefed<StyleRule>
StyleRule::DeclarationChanged(Declaration* aDecl,
                              bool aHandleContainer)
{
  nsRefPtr<StyleRule> clone = new StyleRule(*this, aDecl);

  if (aHandleContainer) {
    nsCSSStyleSheet* sheet = GetStyleSheet();
    if (mParentRule) {
      if (sheet) {
        sheet->ReplaceRuleInGroup(mParentRule, this, clone);
      } else {
        mParentRule->ReplaceStyleRule(this, clone);
      }
    } else if (sheet) {
      sheet->ReplaceStyleRule(this, clone);
    }
  }

  return clone.forget();
}

/* virtual */ void
StyleRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  NS_ABORT_IF_FALSE(mWasMatched,
                    "somebody forgot to call css::StyleRule::RuleMatched");
  mDeclaration->MapNormalRuleInfoInto(aRuleData);
}

#ifdef DEBUG
/* virtual */ void
StyleRule::List(FILE* out, int32_t aIndent) const
{
  // Indent
  for (int32_t index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;
  if (mSelector)
    mSelector->ToString(buffer, GetStyleSheet());

  buffer.AppendLiteral(" ");
  fputs(NS_LossyConvertUTF16toASCII(buffer).get(), out);
  if (nullptr != mDeclaration) {
    mDeclaration->List(out);
  }
  else {
    fputs("{ null declaration }", out);
  }
  fputs("\n", out);
}
#endif

void
StyleRule::GetCssText(nsAString& aCssText)
{
  if (mSelector) {
    mSelector->ToString(aCssText, GetStyleSheet());
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
}

void
StyleRule::SetCssText(const nsAString& aCssText)
{
  // XXX TBI - need to re-parse rule & declaration
}

void
StyleRule::GetSelectorText(nsAString& aSelectorText)
{
  if (mSelector)
    mSelector->ToString(aSelectorText, GetStyleSheet());
  else
    aSelectorText.Truncate();
}

void
StyleRule::SetSelectorText(const nsAString& aSelectorText)
{
  // XXX TBI - get a parser and re-parse the selectors,
  // XXX then need to re-compute the cascade
  // XXX and dirty sheet
}

/* virtual */ size_t
StyleRule::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += mSelector ? mSelector->SizeOfIncludingThis(aMallocSizeOf) : 0;
  n += mDeclaration ? mDeclaration->SizeOfIncludingThis(aMallocSizeOf) : 0;

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mImportantRule;
  // - mDOMRule;

  return n;
}


} // namespace css
} // namespace mozilla
