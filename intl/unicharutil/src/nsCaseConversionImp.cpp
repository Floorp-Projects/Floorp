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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCaseConversionImp.h"
#include "ucdata.h"

#define UCDATAFILEPATH "./res/unicharutil"

#define CAST_PRUNICHAR_TO_ULONG(u) (unsigned long) (((u) & 0x0000FFFF))
#define CAST_ULONG_TO_PRUNICHAR(l) ((PRUnichar) (l))

PRBool nsCaseConversionImp::gInit      = PR_FALSE;

NS_IMPL_ISUPPORTS1(nsCaseConversionImp, nsICaseConversion)

void nsCaseConversionImp::Init()
{
  ucdata_load(UCDATAFILEPATH, UCDATA_CASE|UCDATA_CTYPE);
  gInit = PR_TRUE;
}
nsresult nsCaseConversionImp::ToUpper(
  PRUnichar aChar, PRUnichar* aReturn
)
{
  *aReturn = CAST_ULONG_TO_PRUNICHAR( 
               uctoupper( CAST_PRUNICHAR_TO_ULONG(aChar))
             );
  return NS_OK;
}

nsresult nsCaseConversionImp::ToLower(
  PRUnichar aChar, PRUnichar* aReturn
)
{
  *aReturn = CAST_ULONG_TO_PRUNICHAR( 
                uctolower( CAST_PRUNICHAR_TO_ULONG(aChar))
             );
  return NS_OK;
}

nsresult nsCaseConversionImp::ToTitle(
  PRUnichar aChar, PRUnichar* aReturn
)
{
  *aReturn = CAST_ULONG_TO_PRUNICHAR( 
                uctotitle( CAST_PRUNICHAR_TO_ULONG(aChar))
             );
  return NS_OK;
}

nsresult nsCaseConversionImp::ToUpper(
  const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen
)
{
  PRUint32 i;
  for(i=0;i<aLen;i++)
    aReturn[i] = CAST_ULONG_TO_PRUNICHAR( 
                   uctoupper( CAST_PRUNICHAR_TO_ULONG(anArray[i]))
                 );
  return NS_OK;
}

nsresult nsCaseConversionImp::ToLower(
  const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen
)
{
  PRUint32 i;
  for(i=0;i<aLen;i++)
    aReturn[i] = CAST_ULONG_TO_PRUNICHAR( 
                   uctolower( CAST_PRUNICHAR_TO_ULONG(anArray[i]))
                 );
  return NS_OK;
}

nsresult nsCaseConversionImp::ToTitle(
  const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen
)
{

  PRUint32 i;
  PRBool bLastIsChar = PR_FALSE; 
  for(i=0;i<aLen;i++)
  {
   
    if(bLastIsChar)
      aReturn[i] = CAST_ULONG_TO_PRUNICHAR( 
                     uctolower( CAST_PRUNICHAR_TO_ULONG(anArray[i]))
                   );
    else
      aReturn[i] = CAST_ULONG_TO_PRUNICHAR( 
                     uctotitle( CAST_PRUNICHAR_TO_ULONG(anArray[i]))
                   );

    bLastIsChar = ucisalpha(CAST_PRUNICHAR_TO_ULONG(anArray[i]));
  }
  return NS_OK;
}
   




