/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsICSSStyleRule_h___
#define nsICSSStyleRule_h___

#include <stdio.h>
#include "nslayout.h"
#include "nsIStyleRule.h"
class nsIAtom;
class nsIArena;
class nsString;
class nsICSSDeclaration;


struct nsCSSSelector {
public:
  nsCSSSelector();
  nsCSSSelector(nsIAtom* aTag, nsIAtom* aID, nsIAtom* aClass, nsIAtom* aPseudoClass);
  nsCSSSelector(const nsCSSSelector& aCopy);
  ~nsCSSSelector();

  nsCSSSelector& operator=(const nsCSSSelector& aCopy);
  PRBool Equals(const nsCSSSelector* aOther) const;

  void Set(const nsString& aTag, const nsString& aID, const nsString& aClass, const nsString& aPseudoClass);

public:
  nsIAtom*  mTag;
  nsIAtom*  mID;
  nsIAtom*  mClass; // this'll have to be an array for CSS2
  nsIAtom*  mPseudoClass;

  nsCSSSelector*  mNext;
};

// IID for the nsICSSStyleRule interface {7c277af0-af19-11d1-8031-006008159b5a}
#define NS_ICSS_STYLE_RULE_IID     \
{0x7c277af0, 0xaf19, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsICSSStyleRule : public nsIStyleRule {
public:
  virtual PRBool Equals(const nsIStyleRule* aRule) const = 0;
  virtual PRUint32 HashValue(void) const = 0;

  virtual nsCSSSelector* FirstSelector(void) = 0;
  virtual void AddSelector(const nsCSSSelector& aSelector) = 0;
  virtual void DeleteSelector(nsCSSSelector* aSelector) = 0;

  virtual nsICSSDeclaration* GetDeclaration(void) const = 0;
  virtual void SetDeclaration(nsICSSDeclaration* aDeclaration) = 0;

  virtual PRInt32 GetWeight(void) const = 0;
  virtual void SetWeight(PRInt32 aWeight) = 0;

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;
};

extern NS_HTML nsresult
  NS_NewCSSStyleRule(nsICSSStyleRule** aInstancePtrResult, const nsCSSSelector& aSelector);

#endif /* nsICSSStyleRule_h___ */
