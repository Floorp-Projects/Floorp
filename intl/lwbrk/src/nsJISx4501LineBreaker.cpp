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


#include "nsJISx4501LineBreaker.h"

NS_DEFINE_IID(kILineBreaker, NS_ILINEBREAKER_IID);

NS_IMPL_ISUPPORTS(nsJISx4501LineBreaker, NS_ILINEBREAKER_IID);







nsresult nsJISx4501LineBreaker::BreakInBetween(
  PRUnichar* aText1 , PRUint32 aTextLen1,
  PRUnichar* aText2 , PRUint32 aTextLen2,
  PRBool *oCanBreak)
{

  // to be implement
  return NS_OK;
}


nsresult nsJISx4501LineBreaker::GetLinearIterator(
  PRUnichar* aText, PRUint32 aLen,
  nsILinearIterator** iterator,
  PRBool aForward,
  PRBool aCanBreak)
{
  // to be implement
  return NS_OK;
}

nsresult nsJISx4501LineBreaker::GetBinarySearchIterator(
  PRUnichar* aText, PRUint32 aLen,
  nsIBinarySearchIterator** iterator)
{
  // to be implement
  return NS_OK;
}

nsresult nsJISx4501LineBreaker::GetConnector(
  PRUnichar* aText, PRUint32 aLen,
  nsString& oLinePostfix,
  nsString& oLinePrefix )
{
  // we don't support hyphen, so always return "" as prefix and postfix
  oLinePrefix= "";
  oLinePostfix= "";
  return NS_OK;
}
