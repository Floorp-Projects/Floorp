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
#ifndef nsILineBreaker_h__
#define nsILineBreaker_h__


#include "nsISupports.h"
#include "nsILinearIterator.h"
#include "nsIBinarySearchIterator.h"
#include "nsString.h"

#include "nscore.h"

// {E86B3375-BF89-11d2-B3AF-00805F8A6670}
#define NS_ILINEBREAKER_IID \
{ 0xe86b3375, 0xbf89, 0x11d2, \
    { 0xb3, 0xaf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } };


class nsILineBreaker : public nsISupports
{
  NS_IMETHOD BreakInBetween(PRUnichar* aText1 , PRUint32 aTextLen1,
                                  PRUnichar* aText2 , PRUint32 aTextLen2,
                                  PRBool *oCanBreak) = 0;
  NS_IMETHOD GetLinearIterator(PRUnichar* aText, PRUint32 aLen,
                                      nsILinearIterator** iterator,
                                      PRBool aForward = PR_TRUE,
                                      PRBool aCanBreak = PR_TRUE) = 0;
  NS_IMETHOD GetBinarySearchIterator(PRUnichar* aText, PRUint32 aLen,
                                      nsIBinarySearchIterator** iterator) = 0;
  NS_IMETHOD GetConnector(PRUnichar* aText, PRUint32 aLen,
                                      nsString& oLinePostfix,
                                      nsString& oLinePrefix) = 0;
};


#endif  /* nsILineBreaker_h__ */
