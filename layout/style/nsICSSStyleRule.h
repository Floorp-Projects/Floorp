/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsICSSStyleRule_h___
#define nsICSSStyleRule_h___

//#include <stdio.h>
#include "nslayout.h"
#include "nsICSSRule.h"
#include "nsString.h"

class nsISizeOfHandler;

class nsIAtom;
class nsIArena;
class nsICSSDeclaration;
class nsICSSStyleSheet;

struct nsAtomList {
public:
  nsAtomList(nsIAtom* aAtom);
  nsAtomList(const nsString& aAtomValue);
  nsAtomList(const nsAtomList& aCopy);
  ~nsAtomList(void);
  PRBool Equals(const nsAtomList* aOther) const;

  nsIAtom*    mAtom;
  nsAtomList* mNext;
};

#define NS_ATTR_FUNC_SET        0     // [attr]
#define NS_ATTR_FUNC_EQUALS     1     // [attr=value]
#define NS_ATTR_FUNC_INCLUDES   2     // [attr~=value] (space separated)
#define NS_ATTR_FUNC_DASHMATCH  3     // [attr|=value] ('-' truncated)

struct nsAttrSelector {
public:
  nsAttrSelector(PRInt32 aNameSpace, const nsString& aAttr);
  nsAttrSelector(PRInt32 aNameSpace, const nsString& aAttr, PRUint8 aFunction, 
                 const nsString& aValue, PRBool aCaseSensitive);
  nsAttrSelector(const nsAttrSelector& aCopy);
  ~nsAttrSelector(void);
  PRBool Equals(const nsAttrSelector* aOther) const;

  void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);

  PRInt32         mNameSpace;
  nsIAtom*        mAttr;
  PRUint8         mFunction;
  PRPackedBool    mCaseSensitive;
  nsString        mValue;
  nsAttrSelector* mNext;
};

// Right now, there are three operators:
//   PRUnichar(0), the descendent combinator, is greedy
//   '+' and '>', the adjacent sibling and child combinators, are not
#define NS_IS_GREEDY_OPERATOR(ch) ( ch == PRUnichar(0) )

struct nsCSSSelector {
public:
  nsCSSSelector(void);
  nsCSSSelector(const nsCSSSelector& aCopy);
  ~nsCSSSelector(void);

  nsCSSSelector& operator=(const nsCSSSelector& aCopy);
  PRBool Equals(const nsCSSSelector* aOther) const;

  void Reset(void);
  void SetNameSpace(PRInt32 aNameSpace);
  void SetTag(const nsString& aTag);
  void SetID(const nsString& aID);
  void AddClass(const nsString& aClass);
  void AddPseudoClass(const nsString& aPseudoClass);
  void AddPseudoClass(nsIAtom* aPseudoClass);
  void AddAttribute(PRInt32 aNameSpace, const nsString& aAttr);
  void AddAttribute(PRInt32 aNameSpace, const nsString& aAttr, PRUint8 aFunc, 
                    const nsString& aValue, PRBool aCaseSensitive);
  void SetOperator(PRUnichar aOperator);

  PRInt32 CalcWeight(void) const;

  void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);
  nsresult ToString( nsAWritableString& aString ) const;

public:
  PRInt32         mNameSpace;
  nsIAtom*        mTag;
  nsIAtom*        mID;
  nsAtomList*     mClassList;
  nsAtomList*     mPseudoClassList;
  nsAttrSelector* mAttrList;
  PRUnichar       mOperator;

  nsCSSSelector*  mNext;
};

// IID for the nsICSSStyleRule interface {7c277af0-af19-11d1-8031-006008159b5a}
#define NS_ICSS_STYLE_RULE_IID     \
{0x7c277af0, 0xaf19, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsICSSStyleRule : public nsICSSRule {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICSS_STYLE_RULE_IID; return iid; }

  virtual nsCSSSelector* FirstSelector(void) = 0;
  virtual void AddSelector(const nsCSSSelector& aSelector) = 0;
  virtual void DeleteSelector(nsCSSSelector* aSelector) = 0;
  virtual void SetSourceSelectorText(const nsString& aSelectorText) = 0;
  virtual void GetSourceSelectorText(nsString& aSelectorText) const = 0;

  virtual nsICSSDeclaration* GetDeclaration(void) const = 0;
  virtual void SetDeclaration(nsICSSDeclaration* aDeclaration) = 0;

  virtual PRInt32 GetWeight(void) const = 0;
  virtual void SetWeight(PRInt32 aWeight) = 0;

  virtual nsIStyleRule* GetImportantRule(void) = 0;
};

extern NS_HTML nsresult
  NS_NewCSSStyleRule(nsICSSStyleRule** aInstancePtrResult, const nsCSSSelector& aSelector);

#endif /* nsICSSStyleRule_h___ */
