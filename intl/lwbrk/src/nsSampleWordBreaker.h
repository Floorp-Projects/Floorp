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
#ifndef nsSampleWordBreaker_h__
#define nsSampleWordBreaker_h__


#include "nsIWordBreaker.h"

class nsSampleWordBreaker : public nsIWordBreaker
{
  NS_DECL_ISUPPORTS

  nsSampleWordBreaker() ;
  ~nsSampleWordBreaker() ;

  NS_IMETHOD BreakInBetween(const PRUnichar* aText1 , PRUint32 aTextLen1,
                            const PRUnichar* aText2 , PRUint32 aTextLen2,
                            PRBool *oCanBreak);
  NS_IMETHOD PostionToBoundary(const PRUnichar* aText1 , PRUint32 aTextLen1,
                                      PRUint32 *oWordBegin,
                                      PRUint32 *oWordEnd);

  NS_IMETHOD FirstForwardBreak   (nsIBreakState* state) ; 
  NS_IMETHOD NextForwardBreak    (nsIBreakState* state) ; 

#ifdef AFTER_DOGFOOD 
  NS_IMETHOD FirstBackwardBreak  (nsIBreakState* state) ; 
  NS_IMETHOD NextBackwardBreak   (nsIBreakState* state) ; 
  NS_IMETHOD FirstForwardConnect (nsIBreakState* state) ; 
  NS_IMETHOD NextForwardConnect  (nsIBreakState* state) ; 
  NS_IMETHOD FirstBackwardConnect(nsIBreakState* state) ; 
  NS_IMETHOD NextBackwardConnect (nsIBreakState* state) ; 
#endif 

};

#endif  /* nsSampleWordBreaker_h__ */
