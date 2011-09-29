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
 * representation of CSS style rules (selectors+declaration) and CSS
 * selectors
 */

#ifndef mozilla_css_StyleRule_h__
#define mozilla_css_StyleRule_h__

//#include <stdio.h>
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
  nsAtomList* Clone() const { return Clone(PR_TRUE); }

  nsCOMPtr<nsIAtom> mAtom;
  nsAtomList*       mNext;
private: 
  nsAtomList* Clone(bool aDeep) const;

  // These are not supported and are not implemented! 
  nsAtomList(const nsAtomList& aCopy);
  nsAtomList& operator=(const nsAtomList& aCopy); 
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
  nsPseudoClassList* Clone() const { return Clone(PR_TRUE); }

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

  // These are not supported and are not implemented! 
  nsPseudoClassList(const nsPseudoClassList& aCopy);
  nsPseudoClassList& operator=(const nsPseudoClassList& aCopy); 
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
  nsAttrSelector* Clone() const { return Clone(PR_TRUE); }

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

  // These are not supported and are not implemented! 
  nsAttrSelector(const nsAttrSelector& aCopy);
  nsAttrSelector& operator=(const nsAttrSelector& aCopy); 
};

struct nsCSSSelector {
public:
  nsCSSSelector(void);
  ~nsCSSSelector(void);

  /** Do a deep clone.  Should be used only on the first in the linked list. */
  nsCSSSelector* Clone() const { return Clone(PR_TRUE, PR_TRUE); }

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
  // These are not supported and are not implemented! 
  nsCSSSelector(const nsCSSSelector& aCopy);
  nsCSSSelector& operator=(const nsCSSSelector& aCopy); 
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
  nsCSSSelectorList* Clone() const { return Clone(PR_TRUE); }

  nsCSSSelector*     mSelectors;
  PRInt32            mWeight;
  nsCSSSelectorList* mNext;
private: 
  nsCSSSelectorList* Clone(bool aDeep) const;

  // These are not supported and are not implemented! 
  nsCSSSelectorList(const nsCSSSelectorList& aCopy);
  nsCSSSelectorList& operator=(const nsCSSSelectorList& aCopy); 
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

class NS_FINAL_CLASS StyleRule : public Rule
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

private:
  // This is not supported and not implemented!
  StyleRule& operator=(const StyleRule& aCopy);

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
};

} // namespace css
} // namespace mozilla

NS_DEFINE_STATIC_IID_ACCESSOR(mozilla::css::StyleRule, NS_CSS_STYLE_RULE_IMPL_CID)

#endif /* mozilla_css_StyleRule_h__ */
