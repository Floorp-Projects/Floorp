/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsICSSStyleRule_h___
#define nsICSSStyleRule_h___

//#include <stdio.h>
#include "nsICSSRule.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsCSSProps.h"
#include "nsCSSValue.h"
#include "nsIAtom.h"

class nsISizeOfHandler;

class nsIAtom;
class nsIArena;
class nsCSSDeclaration;
class nsICSSStyleSheet;

struct nsAtomList {
public:
  nsAtomList(nsIAtom* aAtom);
  nsAtomList(const nsString& aAtomValue);
  nsAtomList(const nsAtomList& aCopy);
  ~nsAtomList(void);
  PRBool Equals(const nsAtomList* aOther) const;

  nsCOMPtr<nsIAtom> mAtom;
  nsAtomList*       mNext;
};

struct nsAtomStringList {
public:
  nsAtomStringList(nsIAtom* aAtom, const PRUnichar *aString = nsnull);
  nsAtomStringList(const nsString& aAtomValue, const PRUnichar *aString = nsnull);
  nsAtomStringList(const nsAtomStringList& aCopy);
  ~nsAtomStringList(void);
  PRBool Equals(const nsAtomStringList* aOther) const;

  nsCOMPtr<nsIAtom> mAtom;
  PRUnichar*        mString;
  nsAtomStringList* mNext;
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
  nsAttrSelector(const nsAttrSelector& aCopy);
  ~nsAttrSelector(void);
  PRBool Equals(const nsAttrSelector* aOther) const;

#ifdef DEBUG
  void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);
#endif

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
  void AddID(const nsString& aID);
  void AddClass(const nsString& aClass);
  void AddPseudoClass(const nsString& aPseudoClass, const PRUnichar* aString = nsnull);
  void AddPseudoClass(nsIAtom* aPseudoClass, const PRUnichar* aString = nsnull);
  void AddAttribute(PRInt32 aNameSpace, const nsString& aAttr);
  void AddAttribute(PRInt32 aNameSpace, const nsString& aAttr, PRUint8 aFunc, 
                    const nsString& aValue, PRBool aCaseSensitive);
  void SetOperator(PRUnichar aOperator);

  PRInt32 CalcWeight(void) const;

#ifdef DEBUG
  void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);
#endif
  nsresult ToString( nsAString& aString, nsICSSStyleSheet* aSheet,
                    PRBool aIsPseudoElem, PRInt8 aNegatedIndex ) const;

private:

  void AppendNegationToString(nsAString& aString);

public:
  PRInt32         mNameSpace;
  nsCOMPtr<nsIAtom> mTag;
  nsAtomList*     mIDList;
  nsAtomList*     mClassList;
  nsAtomStringList* mPseudoClassList; // atom for the pseudo, string for
                                      // the argument to functional pseudos
  nsAttrSelector* mAttrList;
  PRUnichar       mOperator;
  nsCSSSelector*  mNegations;

  nsCSSSelector*  mNext;
};


// IID for the nsICSSStyleRule interface {7c277af0-af19-11d1-8031-006008159b5a}
#define NS_ICSS_STYLE_RULE_IID     \
{0x7c277af0, 0xaf19, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsICSSStyleRule : public nsICSSRule {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICSS_STYLE_RULE_IID)

  virtual nsCSSSelector* FirstSelector(void) = 0;
  virtual void AddSelector(const nsCSSSelector& aSelector) = 0;
  virtual void DeleteSelector(nsCSSSelector* aSelector) = 0;
  virtual void SetSourceSelectorText(const nsString& aSelectorText) = 0;
  virtual void GetSourceSelectorText(nsString& aSelectorText) const = 0;

  virtual PRUint32 GetLineNumber(void) const = 0;
  virtual void SetLineNumber(PRUint32 aLineNumber) = 0;

  virtual nsCSSDeclaration* GetDeclaration(void) const = 0;
  virtual void SetDeclaration(nsCSSDeclaration* aDeclaration) = 0;

  virtual PRInt32 GetWeight(void) const = 0;
  virtual void SetWeight(PRInt32 aWeight) = 0;

  virtual already_AddRefed<nsIStyleRule> GetImportantRule(void) = 0;

  // Hook for inspector.
  virtual nsresult GetValue(nsCSSProperty aProperty, nsCSSValue& aValue) = 0;
};

extern NS_EXPORT nsresult
  NS_NewCSSStyleRule(nsICSSStyleRule** aInstancePtrResult, const nsCSSSelector& aSelector);

#endif /* nsICSSStyleRule_h___ */
