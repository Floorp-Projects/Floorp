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
public:

  nsSampleWordBreaker() ;
  virtual ~nsSampleWordBreaker() ;

  NS_IMETHOD BreakInBetween(const PRUnichar* aText1 , PRUint32 aTextLen1,
                            const PRUnichar* aText2 , PRUint32 aTextLen2,
                            PRBool *oCanBreak);
  NS_IMETHOD FindWord(const PRUnichar* aText1 , PRUint32 aTextLen1,
                                      PRUint32 aOffset,
                                      PRUint32 *oWordBegin,
                                      PRUint32 *oWordEnd);

  NS_IMETHOD Next( const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos,
                   PRUint32* oNext, PRBool *oNeedMoreText);

  NS_IMETHOD Prev( const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos,
                   PRUint32* oPrev, PRBool *oNeedMoreText);

protected:
  PRUint8  GetClass(PRUnichar aChar);
  PRUint32 Next(const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos);
  PRUint32 Prev(const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos);


};

#endif  /* nsSampleWordBreaker_h__ */
