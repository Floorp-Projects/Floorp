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
#ifndef nsJISx4501LineBreaker_h__
#define nsJISx4501LineBreaker_h__


#include "nsILineBreaker.h"

class nsJISx4501LineBreaker : public nsILineBreaker
{
  NS_DECL_ISUPPORTS

  nsJISx4501LineBreaker();
  ~nsJISx4501LineBreaker();

  NS_IMETHOD BreakInBetween(PRUnichar* aText1 , PRUint32 aTextLen1,
                            PRUnichar* aText2 , PRUint32 aTextLen2,
                            PRBool *oCanBreak);

  NS_IMETHOD GetLinearIterator(PRUnichar* aText, PRUint32 aLen,
                                      nsILinearIterator** iterator,
                                      PRBool aForward = PR_TRUE,
                                      PRBool aCanBreak = PR_TRUE);

  NS_IMETHOD GetBinarySearchIterator(PRUnichar* aText, PRUint32 aLen,
                                      nsIBinarySearchIterator** iterator);

  NS_IMETHOD GetConnector(PRUnichar* aText, PRUint32 aLen,
                                      nsString& oLinePostfix,
                                      nsString& oLinePrefix);
};

#endif  /* nsJISx4501LineBreaker_h__ */
