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
#include "nsIBreakState.h"

#include "nscore.h"

// {E86B3375-BF89-11d2-B3AF-00805F8A6670}
#define NS_ILINEBREAKER_IID \
{ 0xe86b3375, 0xbf89, 0x11d2, \
    { 0xb3, 0xaf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


class nsILineBreaker : public nsISupports
{
public:
  NS_IMETHOD BreakInBetween(const PRUnichar* aText1 , PRUint32 aTextLen1,
                            const PRUnichar* aText2 , PRUint32 aTextLen2,
                                  PRBool *oCanBreak) = 0;

  NS_IMETHOD Next( const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos,
                   PRUint32* oNext, PRBool *oNeedMoreText) = 0;

  NS_IMETHOD Prev( const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos,
                   PRUint32* oPrev, PRBool *oNeedMoreText) = 0;

};


#endif  /* nsILineBreaker_h__ */
