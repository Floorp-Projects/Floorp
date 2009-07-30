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

#ifndef nsICSSStyleRule_h___
#define nsICSSStyleRule_h___

//#include <stdio.h>
#include "nsICSSRule.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsCSSProps.h"
#include "nsCSSValue.h"
#include "nsIAtom.h"

class nsIAtom;
class nsCSSDeclaration;
class nsICSSStyleSheet;

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
  nsAtomList* Clone(PRBool aDeep) const;

  // These are not supported and are not implemented! 
  nsAtomList(const nsAtomList& aCopy);
  nsAtomList& operator=(const nsAtomList& aCopy); 
};

struct nsPseudoClassList {
public:
  nsPseudoClassList(nsIAtom* aAtom);
  nsPseudoClassList(nsIAtom* aAtom, const PRUnichar *aString);
  nsPseudoClassList(nsIAtom* aAtom, const PRInt32 *aIntPair);
  ~nsPseudoClassList(void);

  /** Do a deep clone.  Should be used only on the first in the linked list. */
  nsPseudoClassList* Clone() const { return Clone(PR_TRUE); }

  nsCOMPtr<nsIAtom> mAtom;
  union {
    // For a given value of mAtom, we have either:
    //   a. no value, which means mMemory is always null
    //      (if neither of the conditions for (b) or (c) is true)
    //   b. a string value, which means mString/mMemory is non-null
    //      (if nsCSSPseudoClasses::HasStringArg(mAtom))
    //   c. an integer pair value, which means mNumbers/mMemory is non-null
    //      (if nsCSSPseudoClasses::HasNthPairArg(mAtom))
    void*           mMemory; // both pointer types use NS_Alloc/NS_Free
    PRUnichar*      mString;
    PRInt32*        mNumbers;
  } u;
  nsPseudoClassList* mNext;
private: 
  nsPseudoClassList* Clone(PRBool aDeep) const;

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
                 const nsString& aValue, PRBool aCaseSensitive);
  nsAttrSelector(PRInt32 aNameSpace, nsIAtom* aAttr, PRUint8 aFunction, 
                 const nsString& aValue, PRBool aCaseSensitive);
  ~nsAttrSelector(void);

  /** Do a deep clone.  Should be used only on the first in the linked list. */
  nsAttrSelector* Clone() const { return Clone(PR_TRUE); }

  nsString        mValue;
  nsAttrSelector* mNext;
  nsCOMPtr<nsIAtom> mAttr;
  PRInt32         mNameSpace;
  PRUint8         mFunction;
  PRPackedBool    mCaseSensitive;
private: 
  nsAttrSelector* Clone(PRBool aDeep) const;

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
  void SetTag(const nsString& aTag, PRBool aCaseSensitive);
  void AddID(const nsString& aID);
  void AddClass(const nsString& aClass);
  void AddPseudoClass(nsIAtom* aPseudoClass);
  void AddPseudoClass(nsIAtom* aPseudoClass, const PRUnichar* aString);
  void AddPseudoClass(nsIAtom* aPseudoClass, const PRInt32* aIntPair);
  void AddAttribute(PRInt32 aNameSpace, const nsString& aAttr);
  void AddAttribute(PRInt32 aNameSpace, const nsString& aAttr, PRUint8 aFunc, 
                    const nsString& aValue, PRBool aCaseSensitive);
  void SetOperator(PRUnichar aOperator);

  inline PRBool HasTagSelector() const {
    return !!mCasedTag;
  }

  inline PRBool IsPseudoElement() const {
    return mLowercaseTag && !mCasedTag;
  }

  // Calculate the specificity of this selector (not including its mNext!).
  PRInt32 CalcWeight() const;

  void ToString(nsAString& aString, nsICSSStyleSheet* aSheet,
                PRBool aAppend = PR_FALSE) const;

private:
  void AddPseudoClassInternal(nsPseudoClassList *aPseudoClass);
  nsCSSSelector* Clone(PRBool aDeepNext, PRBool aDeepNegations) const;

  void AppendToStringWithoutCombinators(nsAString& aString,
                                        nsICSSStyleSheet* aSheet) const;
  void AppendToStringWithoutCombinatorsOrNegations(nsAString& aString,
                                                   nsICSSStyleSheet* aSheet,
                                                   PRBool aIsNegated)
                                                        const;
  // Returns true if this selector can have a namespace specified (which
  // happens if and only if the default namespace would apply to this
  // selector).
  PRBool CanBeNamespaced(PRBool aIsNegated) const;
  // Calculate the specificity of this selector (not including its mNext
  // or its mNegations).
  PRInt32 CalcWeightWithoutNegations() const;

public:
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
   * Push the selector pointed to by |aSelector| on to the beginning of
   * |mSelectors|, setting its |mNext| to the current value of |mSelectors|.
   * This nulls out aSelector.
   *
   * The caller is responsible for updating |mWeight|.
   */
  void AddSelector(nsAutoPtr<nsCSSSelector>& aSelector);

  /**
   * Should be used only on the first in the list
   */
  void ToString(nsAString& aResult, nsICSSStyleSheet* aSheet);

  /**
   * Do a deep clone.  Should be used only on the first in the list.
   */
  nsCSSSelectorList* Clone() const { return Clone(PR_TRUE); }

  nsCSSSelector*     mSelectors;
  PRInt32            mWeight;
  nsCSSSelectorList* mNext;
private: 
  nsCSSSelectorList* Clone(PRBool aDeep) const;

  // These are not supported and are not implemented! 
  nsCSSSelectorList(const nsCSSSelectorList& aCopy);
  nsCSSSelectorList& operator=(const nsCSSSelectorList& aCopy); 
};

// IID for the nsICSSStyleRule interface {00803ccc-66e8-4ec8-a037-45e901bb5304}
#define NS_ICSS_STYLE_RULE_IID     \
{0x00803ccc, 0x66e8, 0x4ec8, {0xa0, 0x37, 0x45, 0xe9, 0x01, 0xbb, 0x53, 0x04}}

class nsICSSStyleRule : public nsICSSRule {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICSS_STYLE_RULE_IID)

  // null for style attribute
  virtual nsCSSSelectorList* Selector(void) = 0;

  virtual PRUint32 GetLineNumber(void) const = 0;
  virtual void SetLineNumber(PRUint32 aLineNumber) = 0;

  virtual nsCSSDeclaration* GetDeclaration(void) const = 0;

  /**
   * Return a new |nsIStyleRule| instance that replaces the current one,
   * due to a change in the |nsCSSDeclaration|.  Due to the
   * |nsIStyleRule| contract of immutability, this must be called if the
   * declaration is modified.
   *
   * |DeclarationChanged| handles replacing the object in the container
   * sheet or group rule if |aHandleContainer| is true.
   */
  virtual already_AddRefed<nsICSSStyleRule>
    DeclarationChanged(PRBool aHandleContainer) = 0;

  virtual already_AddRefed<nsIStyleRule> GetImportantRule(void) = 0;

  // hooks for DOM rule
  virtual nsresult GetCssText(nsAString& aCssText) = 0;
  virtual nsresult SetCssText(const nsAString& aCssText) = 0;
  virtual nsresult GetParentStyleSheet(nsICSSStyleSheet** aSheet) = 0;
  virtual nsresult GetParentRule(nsICSSGroupRule** aParentRule) = 0;
  virtual nsresult GetSelectorText(nsAString& aSelectorText) = 0;
  virtual nsresult SetSelectorText(const nsAString& aSelectorText) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICSSStyleRule, NS_ICSS_STYLE_RULE_IID)

nsresult
NS_NewCSSStyleRule(nsICSSStyleRule** aInstancePtrResult,
                   nsCSSSelectorList* aSelector,
                   nsCSSDeclaration* aDeclaration);

#endif /* nsICSSStyleRule_h___ */
