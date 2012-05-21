/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * representation of CSS style rules (selectors+declaration) and CSS
 * selectors
 */

#ifndef mozilla_css_StyleRule_h__
#define mozilla_css_StyleRule_h__

#include "mozilla/Attributes.h"

#include "mozilla/Attributes.h"
#include "mozilla/css/Rule.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSPseudoClasses.h"
#include "nsAutoPtr.h"

class nsIAtom;
class nsCSSStyleSheet;
struct nsCSSSelectorList;
class nsCSSCompressedDataBlock;

struct nsAtomList {
public:
  nsAtomList(nsIAtom* aAtom);
  nsAtomList(const nsString& aAtomValue);
  ~nsAtomList(void);

  /** Do a deep clone.  Should be used only on the first in the linked list. */
  nsAtomList* Clone() const { return Clone(true); }

  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

  nsCOMPtr<nsIAtom> mAtom;
  nsAtomList*       mNext;
private: 
  nsAtomList* Clone(bool aDeep) const;

  nsAtomList(const nsAtomList& aCopy) MOZ_DELETE;
  nsAtomList& operator=(const nsAtomList& aCopy) MOZ_DELETE;
};

struct nsPseudoClassList {
public:
  nsPseudoClassList(nsCSSPseudoClasses::Type aType);
  nsPseudoClassList(nsCSSPseudoClasses::Type aType, const PRUnichar *aString);
  nsPseudoClassList(nsCSSPseudoClasses::Type aType, const PRInt32 *aIntPair);
  nsPseudoClassList(nsCSSPseudoClasses::Type aType,
                    nsCSSSelectorList *aSelectorList /* takes ownership */);
  ~nsPseudoClassList(void);

  /** Do a deep clone.  Should be used only on the first in the linked list. */
  nsPseudoClassList* Clone() const { return Clone(true); }

  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

  union {
    // For a given value of mType, we have either:
    //   a. no value, which means mMemory is always null
    //      (if none of the conditions for (b), (c), or (d) is true)
    //   b. a string value, which means mString/mMemory is non-null
    //      (if nsCSSPseudoClasses::HasStringArg(mType))
    //   c. an integer pair value, which means mNumbers/mMemory is non-null
    //      (if nsCSSPseudoClasses::HasNthPairArg(mType))
    //   d. a selector list, which means mSelectors is non-null
    //      (if nsCSSPseudoClasses::HasSelectorListArg(mType))
    void*           mMemory; // mString and mNumbers use NS_Alloc/NS_Free
    PRUnichar*      mString;
    PRInt32*        mNumbers;
    nsCSSSelectorList* mSelectors;
  } u;
  nsCSSPseudoClasses::Type mType;
  nsPseudoClassList* mNext;
private: 
  nsPseudoClassList* Clone(bool aDeep) const;

  nsPseudoClassList(const nsPseudoClassList& aCopy) MOZ_DELETE;
  nsPseudoClassList& operator=(const nsPseudoClassList& aCopy) MOZ_DELETE;
};

#define NS_ATTR_FUNC_SET        0     // [attr]
#define NS_ATTR_FUNC_EQUALS     1     // [attr=value]
#define NS_ATTR_FUNC_INCLUDES   2     // [attr~=value] (space separated)
#define NS_ATTR_FUNC_DASHMATCH  3     // [attr|=value] ('-' truncated)
#define NS_ATTR_FUNC_BEGINSMATCH  4   // [attr^=value] (begins with)
#define NS_ATTR_FUNC_ENDSMATCH  5     // [attr$=value] (ends with)
#define NS_ATTR_FUNC_CONTAINSMATCH 6  // [attr*=value] (contains substring)

struct nsAttrSelector {
public:
  nsAttrSelector(PRInt32 aNameSpace, const nsString& aAttr);
  nsAttrSelector(PRInt32 aNameSpace, const nsString& aAttr, PRUint8 aFunction, 
                 const nsString& aValue, bool aCaseSensitive);
  nsAttrSelector(PRInt32 aNameSpace, nsIAtom* aLowercaseAttr, 
                 nsIAtom* aCasedAttr, PRUint8 aFunction, 
                 const nsString& aValue, bool aCaseSensitive);
  ~nsAttrSelector(void);

  /** Do a deep clone.  Should be used only on the first in the linked list. */
  nsAttrSelector* Clone() const { return Clone(true); }

  nsString        mValue;
  nsAttrSelector* mNext;
  nsCOMPtr<nsIAtom> mLowercaseAttr;
  nsCOMPtr<nsIAtom> mCasedAttr;
  PRInt32         mNameSpace;
  PRUint8         mFunction;
  bool            mCaseSensitive; // If we are in an HTML document,
                                  // is the value case sensitive?
private: 
  nsAttrSelector* Clone(bool aDeep) const;

  nsAttrSelector(const nsAttrSelector& aCopy) MOZ_DELETE;
  nsAttrSelector& operator=(const nsAttrSelector& aCopy) MOZ_DELETE;
};

struct nsCSSSelector {
public:
  nsCSSSelector(void);
  ~nsCSSSelector(void);

  /** Do a deep clone.  Should be used only on the first in the linked list. */
  nsCSSSelector* Clone() const { return Clone(true, true); }

  void Reset(void);
  void SetNameSpace(PRInt32 aNameSpace);
  void SetTag(const nsString& aTag);
  void AddID(const nsString& aID);
  void AddClass(const nsString& aClass);
  void AddPseudoClass(nsCSSPseudoClasses::Type aType);
  void AddPseudoClass(nsCSSPseudoClasses::Type aType, const PRUnichar* aString);
  void AddPseudoClass(nsCSSPseudoClasses::Type aType, const PRInt32* aIntPair);
  // takes ownership of aSelectorList
  void AddPseudoClass(nsCSSPseudoClasses::Type aType,
                      nsCSSSelectorList* aSelectorList);
  void AddAttribute(PRInt32 aNameSpace, const nsString& aAttr);
  void AddAttribute(PRInt32 aNameSpace, const nsString& aAttr, PRUint8 aFunc, 
                    const nsString& aValue, bool aCaseSensitive);
  void SetOperator(PRUnichar aOperator);

  inline bool HasTagSelector() const {
    return !!mCasedTag;
  }

  inline bool IsPseudoElement() const {
    return mLowercaseTag && !mCasedTag;
  }

  // Calculate the specificity of this selector (not including its mNext!).
  PRInt32 CalcWeight() const;

  void ToString(nsAString& aString, nsCSSStyleSheet* aSheet,
                bool aAppend = false) const;

private:
  void AddPseudoClassInternal(nsPseudoClassList *aPseudoClass);
  nsCSSSelector* Clone(bool aDeepNext, bool aDeepNegations) const;

  void AppendToStringWithoutCombinators(nsAString& aString,
                                        nsCSSStyleSheet* aSheet) const;
  void AppendToStringWithoutCombinatorsOrNegations(nsAString& aString,
                                                   nsCSSStyleSheet* aSheet,
                                                   bool aIsNegated)
                                                        const;
  // Returns true if this selector can have a namespace specified (which
  // happens if and only if the default namespace would apply to this
  // selector).
  bool CanBeNamespaced(bool aIsNegated) const;
  // Calculate the specificity of this selector (not including its mNext
  // or its mNegations).
  PRInt32 CalcWeightWithoutNegations() const;

public:
  // Get and set the selector's pseudo type
  nsCSSPseudoElements::Type PseudoType() const {
    return static_cast<nsCSSPseudoElements::Type>(mPseudoType);
  }
  void SetPseudoType(nsCSSPseudoElements::Type aType) {
    NS_ASSERTION(aType > PR_INT16_MIN && aType < PR_INT16_MAX, "Out of bounds");
    mPseudoType = static_cast<PRInt16>(aType);
  }

  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

  // For case-sensitive documents, mLowercaseTag is the same as mCasedTag,
  // but in case-insensitive documents (HTML) mLowercaseTag is lowercase.
  // Also, for pseudo-elements mCasedTag will be null but mLowercaseTag
  // contains their name.
  nsCOMPtr<nsIAtom> mLowercaseTag;
  nsCOMPtr<nsIAtom> mCasedTag;
  nsAtomList*     mIDList;
  nsAtomList*     mClassList;
  nsPseudoClassList* mPseudoClassList; // atom for the pseudo, string for
                                       // the argument to functional pseudos
  nsAttrSelector* mAttrList;
  nsCSSSelector*  mNegations;
  nsCSSSelector*  mNext;
  PRInt32         mNameSpace;
  PRUnichar       mOperator;
private:
  // PRInt16 to make sure it packs well with mOperator
  PRInt16        mPseudoType;

  nsCSSSelector(const nsCSSSelector& aCopy) MOZ_DELETE;
  nsCSSSelector& operator=(const nsCSSSelector& aCopy) MOZ_DELETE;
};

/**
 * A selector list is the unit of selectors that each style rule has.
 * For example, "P B, H1 B { ... }" would be a selector list with two
 * items (where each |nsCSSSelectorList| object's |mSelectors| has
 * an |mNext| for the P or H1).  We represent them as linked lists.
 */
struct nsCSSSelectorList {
  nsCSSSelectorList(void);
  ~nsCSSSelectorList(void);

  /**
   * Create a new selector and push it onto the beginning of |mSelectors|,
   * setting its |mNext| to the current value of |mSelectors|.  If there is an
   * earlier selector, set its |mOperator| to |aOperator|; else |aOperator|
   * must be PRUnichar(0).
   * Returns the new selector.
   * The list owns the new selector.
   * The caller is responsible for updating |mWeight|.
   */
  nsCSSSelector* AddSelector(PRUnichar aOperator);

  /**
   * Should be used only on the first in the list
   */
  void ToString(nsAString& aResult, nsCSSStyleSheet* aSheet);

  /**
   * Do a deep clone.  Should be used only on the first in the list.
   */
  nsCSSSelectorList* Clone() const { return Clone(true); }

  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

  nsCSSSelector*     mSelectors;
  PRInt32            mWeight;
  nsCSSSelectorList* mNext;
private: 
  nsCSSSelectorList* Clone(bool aDeep) const;

  nsCSSSelectorList(const nsCSSSelectorList& aCopy) MOZ_DELETE;
  nsCSSSelectorList& operator=(const nsCSSSelectorList& aCopy) MOZ_DELETE;
};

// 464bab7a-2fce-4f30-ab44-b7a5f3aae57d
#define NS_CSS_STYLE_RULE_IMPL_CID \
{ 0x464bab7a, 0x2fce, 0x4f30, \
  { 0xab, 0x44, 0xb7, 0xa5, 0xf3, 0xaa, 0xe5, 0x7d } }

namespace mozilla {
namespace css {

class Declaration;
class DOMCSSStyleRule;

class StyleRule;

class ImportantRule : public nsIStyleRule {
public:
  ImportantRule(Declaration *aDeclaration);

  NS_DECL_ISUPPORTS

  // nsIStyleRule interface
  virtual void MapRuleInfoInto(nsRuleData* aRuleData);
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

protected:
  virtual ~ImportantRule();

  // Not an owning reference; the StyleRule that owns this
  // ImportantRule also owns the mDeclaration, and any rule node
  // pointing to this rule keeps that StyleRule alive as well.
  Declaration* mDeclaration;

  friend class StyleRule;
};

class StyleRule MOZ_FINAL : public Rule
{
 public:
  StyleRule(nsCSSSelectorList* aSelector,
            Declaration *aDeclaration);
private:
  // for |Clone|
  StyleRule(const StyleRule& aCopy);
  // for |DeclarationChanged|
  StyleRule(StyleRule& aCopy,
            Declaration *aDeclaration);
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CSS_STYLE_RULE_IMPL_CID)

  NS_DECL_ISUPPORTS_INHERITED

  // null for style attribute
  nsCSSSelectorList* Selector() { return mSelector; }

  PRUint32 GetLineNumber() const { return mLineNumber; }
  void SetLineNumber(PRUint32 aLineNumber) { mLineNumber = aLineNumber; }

  Declaration* GetDeclaration() const { return mDeclaration; }

  /**
   * Return a new |nsIStyleRule| instance that replaces the current
   * one, with |aDecl| replacing the previous declaration. Due to the
   * |nsIStyleRule| contract of immutability, this must be called if
   * the declaration is modified.
   *
   * |DeclarationChanged| handles replacing the object in the container
   * sheet or group rule if |aHandleContainer| is true.
   */
  already_AddRefed<StyleRule>
  DeclarationChanged(Declaration* aDecl, bool aHandleContainer);

  nsIStyleRule* GetImportantRule() const { return mImportantRule; }

  /**
   * The rule processor must call this method before calling
   * nsRuleWalker::Forward on this rule during rule matching.
   */
  void RuleMatched();

  // hooks for DOM rule
  void GetCssText(nsAString& aCssText);
  void SetCssText(const nsAString& aCssText);
  void GetSelectorText(nsAString& aSelectorText);
  void SetSelectorText(const nsAString& aSelectorText);

  virtual PRInt32 GetType() const;

  virtual already_AddRefed<Rule> Clone() const;

  virtual nsIDOMCSSRule* GetDOMRule();

  // The new mapping function.
  virtual void MapRuleInfoInto(nsRuleData* aRuleData);

#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  virtual size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

private:
  ~StyleRule();

private:
  nsCSSSelectorList*      mSelector; // null for style attribute
  Declaration*            mDeclaration;
  ImportantRule*          mImportantRule; // initialized by RuleMatched
  DOMCSSStyleRule*        mDOMRule;
  // Keep the same type so that MSVC packs them.
  PRUint32                mLineNumber : 31;
  PRUint32                mWasMatched : 1;

private:
  StyleRule& operator=(const StyleRule& aCopy) MOZ_DELETE;
};

} // namespace css
} // namespace mozilla

NS_DEFINE_STATIC_IID_ACCESSOR(mozilla::css::StyleRule, NS_CSS_STYLE_RULE_IMPL_CID)

#endif /* mozilla_css_StyleRule_h__ */
