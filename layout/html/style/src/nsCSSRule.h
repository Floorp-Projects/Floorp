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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsCSSRule_h___
#define nsCSSRule_h___

#include "nsISupports.h"

class nsIArena;
class nsIStyleSheet;
class nsICSSStyleSheet;
class nsIStyleContext;
class nsIPresContext;

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

  // nsIStyleRule methods
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;

  NS_IMETHOD MapFontStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext);
  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext);

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;

  nsICSSStyleSheet*   mSheet;                         

#ifdef DEBUG_REFS
  PRInt32 mInstance;
#endif
};

#endif /* nsCSSRule_h___ */
