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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsCSSRule_h___
#define nsCSSRule_h___

#include "nsISupports.h"

class nsIArena;
class nsIStyleSheet;
class nsICSSStyleSheet;
class nsIPresContext;
struct nsRuleData;
class nsICSSGroupRule;

class nsCSSRule {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  nsCSSRule(void);
  nsCSSRule(const nsCSSRule& aCopy);
  virtual ~nsCSSRule(void);

  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
  NS_IMETHOD SetStyleSheet(nsICSSStyleSheet* aSheet);

  NS_IMETHOD SetParentRule(nsICSSGroupRule* aRule);

  // nsIStyleRule methods
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;

  // The new mapping function.
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;
  NS_DECL_OWNINGTHREAD // for thread-safety checking

  nsICSSStyleSheet*   mSheet;                         
  nsICSSGroupRule*    mParentRule;
#ifdef DEBUG_REFS
  PRInt32 mInstance;
#endif
};

#endif /* nsCSSRule_h___ */
