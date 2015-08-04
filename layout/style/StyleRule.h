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
#include "mozilla/MemoryReporting.h"
#include "mozilla/css/Rule.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSPseudoClasses.h"

class nsIAtom;
struct nsCSSSelectorList;

namespace mozilla {
class CSSStyleSheet;
} // namespace mozilla

struct nsAtomList {
public:
  explicit nsAtomList(nsIAtom* aAtom);
  explicit nsAtomList(const nsString& aAtomValue);
  ~nsAtomList(void);

  /** Do a deep clone.  Should be used only on the first in the linked list. */
  nsAtomList* Clone() const { return Clone(true); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  nsCOMPtr<nsIAtom> mAtom;
  nsAtomList*       mNext;
private: 
  nsAtomList* Clone(bool aDeep) const;

  nsAtomList(const nsAtomList& aCopy) = delete;
  nsAtomList& operator=(const nsAtomList& aCopy) = delete;
};

struct nsPseudoClassList {
public:
  explicit nsPseudoClassList(nsCSSPseudoClasses::Type aType);
  nsPseudoClassList(nsCSSPseudoClasses::Type aType, const char16_t *aString);
  nsPseudoClassList(nsCSSPseudoClasses::Type aType, const int32_t *aIntPair);
  nsPseudoClassList(nsCSSPseudoClasses::Type aType,
                    nsCSSSelectorList *aSelectorList /* takes ownership */);
  ~nsPseudoClassList(void);

  /** Do a deep clone.  Should be used only on the first in the linked list. */
  nsPseudoClassList* Clone() const { return Clone(true); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

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
    void*           mMemory; // mString and mNumbers use moz_xmalloc/free
    char16_t*      mString;
    int32_t*        mNumbers;
    nsCSSSelectorList* mSelectors;
  } u;
  nsCSSPseudoClasses::Type mType;
  nsPseudoClassList* mNext;
private: 
  nsPseudoClassList* Clone(bool aDeep) const;

  nsPseudoClassList(const nsPseudoClassList& aCopy) = delete;
  nsPseudoClassList& operator=(const nsPseudoClassList& aCopy) = delete;
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
  nsAttrSelector(int32_t aNameSpace, const nsString& aAttr);
  nsAttrSelector(int32_t aNameSpace, const nsString& aAttr, uint8_t aFunction, 
                 const nsString& aValue, bool aCaseSensitive);
  nsAttrSelector(int32_t aNameSpace, nsIAtom* aLowercaseAttr, 
                 nsIAtom* aCasedAttr, uint8_t aFunction, 
                 const nsString& aValue, bool aCaseSensitive);
  ~nsAttrSelector(void);

  /** Do a deep clone.  Should be used only on the first in the linked list. */
  nsAttrSelector* Clone() const { return Clone(true); }

  nsString        mValue;
  nsAttrSelector* mNext;
  nsCOMPtr<nsIAtom> mLowercaseAttr;
  nsCOMPtr<nsIAtom> mCasedAttr;
  int32_t         mNameSpace;
  uint8_t         mFunction;
  bool            mCaseSensitive; // If we are in an HTML document,
                                  // is the value case sensitive?
private: 
  nsAttrSelector* Clone(bool aDeep) const;

  nsAttrSelector(const nsAttrSelector& aCopy) = delete;
  nsAttrSelector& operator=(const nsAttrSelector& aCopy) = delete;
};

struct nsCSSSelector {
public:
  nsCSSSelector(void);
  ~nsCSSSelector(void);

  /** Do a deep clone.  Should be used only on the first in the linked list. */
  nsCSSSelector* Clone() const { return Clone(true, true); }

  void Reset(void);
  void SetNameSpace(int32_t aNameSpace);
  void SetTag(const nsString& aTag);
  void AddID(const nsString& aID);
  void AddClass(const nsString& aClass);
  void AddPseudoClass(nsCSSPseudoClasses::Type aType);
  void AddPseudoClass(nsCSSPseudoClasses::Type aType, const char16_t* aString);
  void AddPseudoClass(nsCSSPseudoClasses::Type aType, const int32_t* aIntPair);
  // takes ownership of aSelectorList
  void AddPseudoClass(nsCSSPseudoClasses::Type aType,
                      nsCSSSelectorList* aSelectorList);
  void AddAttribute(int32_t aNameSpace, const nsString& aAttr);
  void AddAttribute(int32_t aNameSpace, const nsString& aAttr, uint8_t aFunc, 
                    const nsString& aValue, bool aCaseSensitive);
  void SetOperator(char16_t aOperator);

  inline bool HasTagSelector() const {
    return !!mCasedTag;
  }

  inline bool IsPseudoElement() const {
    return mLowercaseTag && !mCasedTag;
  }

  // Calculate the specificity of this selector (not including its mNext!).
  int32_t CalcWeight() const;

  void ToString(nsAString& aString, mozilla::CSSStyleSheet* aSheet,
                bool aAppend = false) const;

private:
  void AddPseudoClassInternal(nsPseudoClassList *aPseudoClass);
  nsCSSSelector* Clone(bool aDeepNext, bool aDeepNegations) const;

  void AppendToStringWithoutCombinators(nsAString& aString,
                                        mozilla::CSSStyleSheet* aSheet) const;
  void AppendToStringWithoutCombinatorsOrNegations(nsAString& aString,
                                                   mozilla::CSSStyleSheet* aSheet,
                                                   bool aIsNegated)
                                                        const;
  // Returns true if this selector can have a namespace specified (which
  // happens if and only if the default namespace would apply to this
  // selector).
  bool CanBeNamespaced(bool aIsNegated) const;
  // Calculate the specificity of this selector (not including its mNext
  // or its mNegations).
  int32_t CalcWeightWithoutNegations() const;

public:
  // Get and set the selector's pseudo type
  nsCSSPseudoElements::Type PseudoType() const {
    return static_cast<nsCSSPseudoElements::Type>(mPseudoType);
  }
  void SetPseudoType(nsCSSPseudoElements::Type aType) {
    NS_ASSERTION(static_cast<int32_t>(aType) >= INT16_MIN &&
                 static_cast<int32_t>(aType) <= INT16_MAX,
                 "Out of bounds - this will overflow mPseudoType");
    mPseudoType = static_cast<int16_t>(aType);
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

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
  int32_t         mNameSpace;
  char16_t       mOperator;
private:
  // int16_t to make sure it packs well with mOperator
  int16_t        mPseudoType;

  nsCSSSelector(const nsCSSSelector& aCopy) = delete;
  nsCSSSelector& operator=(const nsCSSSelector& aCopy) = delete;
};

/**
 * A selector list is the unit of selectors that each style rule has.
 * For example, "P B, H1 B { ... }" would be a selector list with two
 * items (where each |nsCSSSelectorList| object's |mSelectors| has
 * an |mNext| for the P or H1).  We represent them as linked lists.
 */
class inDOMUtils;

struct nsCSSSelectorList {
  nsCSSSelectorList(void);
  ~nsCSSSelectorList(void);

  /**
   * Create a new selector and push it onto the beginning of |mSelectors|,
   * setting its |mNext| to the current value of |mSelectors|.  If there is an
   * earlier selector, set its |mOperator| to |aOperator|; else |aOperator|
   * must be char16_t(0).
   * Returns the new selector.
   * The list owns the new selector.
   * The caller is responsible for updating |mWeight|.
   */
  nsCSSSelector* AddSelector(char16_t aOperator);

  /**
   * Point |mSelectors| to its |mNext|, and delete the first node in the old
   * |mSelectors|.
   * Should only be used on a list with more than one selector in it.
   */
  void RemoveRightmostSelector();

  /**
   * Should be used only on the first in the list
   */
  void ToString(nsAString& aResult, mozilla::CSSStyleSheet* aSheet);

  /**
   * Do a deep clone.  Should be used only on the first in the list.
   */
  nsCSSSelectorList* Clone() const { return Clone(true); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  nsCSSSelector*     mSelectors;
  int32_t            mWeight;
  nsCSSSelectorList* mNext;
protected:
  friend class inDOMUtils;
  nsCSSSelectorList* Clone(bool aDeep) const;

private:
  nsCSSSelectorList(const nsCSSSelectorList& aCopy) = delete;
  nsCSSSelectorList& operator=(const nsCSSSelectorList& aCopy) = delete;
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

class ImportantRule final : public nsIStyleRule {
public:
  explicit ImportantRule(Declaration *aDeclaration);

  NS_DECL_ISUPPORTS

  // nsIStyleRule interface
  virtual void MapRuleInfoInto(nsRuleData* aRuleData) override;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif

protected:
  virtual ~ImportantRule();

  // Not an owning reference; the StyleRule that owns this
  // ImportantRule also owns the mDeclaration, and any rule node
  // pointing to this rule keeps that StyleRule alive as well.
  Declaration* mDeclaration;

  friend class StyleRule;
};

class StyleRule final : public Rule
{
 public:
  StyleRule(nsCSSSelectorList* aSelector,
            Declaration *aDeclaration,
            uint32_t aLineNumber, uint32_t aColumnNumber);
private:
  // for |Clone|
  StyleRule(const StyleRule& aCopy);
  // for |DeclarationChanged|
  StyleRule(StyleRule& aCopy,
            Declaration *aDeclaration);
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CSS_STYLE_RULE_IMPL_CID)

  NS_DECL_ISUPPORTS

  // null for style attribute
  nsCSSSelectorList* Selector() { return mSelector; }

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

  virtual int32_t GetType() const override;

  virtual already_AddRefed<Rule> Clone() const override;

  virtual nsIDOMCSSRule* GetDOMRule() override;

  virtual nsIDOMCSSRule* GetExistingDOMRule() override;

  // The new mapping function.
  virtual void MapRuleInfoInto(nsRuleData* aRuleData) override;

#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

private:
  ~StyleRule();

private:
  nsCSSSelectorList*      mSelector; // null for style attribute
  Declaration*            mDeclaration;
  nsRefPtr<ImportantRule> mImportantRule; // initialized by RuleMatched
  nsRefPtr<DOMCSSStyleRule> mDOMRule;

private:
  StyleRule& operator=(const StyleRule& aCopy) = delete;
};

NS_DEFINE_STATIC_IID_ACCESSOR(StyleRule, NS_CSS_STYLE_RULE_IMPL_CID)

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_StyleRule_h__ */
