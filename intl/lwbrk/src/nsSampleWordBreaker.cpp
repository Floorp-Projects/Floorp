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


#include "nsSampleWordBreaker.h"

#include "pratom.h"
#include "nsLWBRKDll.h"
nsSampleWordBreaker::nsSampleWordBreaker()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}
nsSampleWordBreaker::~nsSampleWordBreaker()
{
  PR_AtomicDecrement(&g_InstanceCount);
}

NS_DEFINE_IID(kIWordBreakerIID, NS_IWORDBREAKER_IID);

NS_IMPL_ISUPPORTS(nsSampleWordBreaker, kIWordBreakerIID);

nsresult nsSampleWordBreaker::BreakInBetween(
  const PRUnichar* aText1 , PRUint32 aTextLen1,
  const PRUnichar* aText2 , PRUint32 aTextLen2,
  PRBool *oCanBreak)
{
  NS_PRECONDITION( nsnull != aText1, "null ptr");
  NS_PRECONDITION( nsnull != aText2, "null ptr");

  if((aText1 == nsnull) || (aText2 == nsnull))
    return NS_ERROR_NULL_POINTER; 

  if( (0 == aTextLen1) || (0 == aTextLen2))
  {
    *oCanBreak = PR_FALSE; 
    return NS_OK;
  }

  *oCanBreak = (this->GetClass(aText1[aTextLen1-1]) != this->GetClass(aText2[0]));

  return NS_OK;
}


// hack
typedef enum {
  kWbClassSpace = 0,
  kWbClassAlphaLetter,
  kWbClassPunct,
  kWbClassHanLetter,
  kWbClassKatakanaLetter,
  kWbClassHiraganaLetter
} wb_class;

#define IS_ASCII(c)            (0 != ( 0x7f & (c)))
#define ASCII_IS_ALPHA(c)         ((( 'a' <= (c)) && ((c) <= 'z')) || (( 'A' <= (c)) && ((c) <= 'Z')))
#define ASCII_IS_DIGIT(c)         (( '0' <= (c)) && ((c) <= '9'))
#define ASCII_IS_SPACE(c)         (( ' ' == (c)) || ( '\t' == (c)) || ( '\r' == (c)) || ( '\n' == (c)))

#define IS_HAN(c)              (( 0x4e00 <= (c)) && ((c) <= 0x9fff))||(( 0xf900 <= (c)) && ((c) <= 0xfaff))
#define IS_KATAKANA(c)         (( 0x30A0 <= (c)) && ((c) <= 0x30FF))
#define IS_HIRAGANA(c)         (( 0x3040 <= (c)) && ((c) <= 0x309F))

PRUint8 nsSampleWordBreaker::GetClass(PRUnichar c)
{
  // begin of the hack
  if(IS_ASCII(c))
  {
    if(ASCII_IS_SPACE(c))
      return kWbClassSpace;
    else if(ASCII_IS_ALPHA(c) || ASCII_IS_DIGIT(c))
      return kWbClassAlphaLetter;
    else
      return kWbClassPunct;
  }
  else if(IS_HAN(c)) {
      return kWbClassHanLetter;
  }
  else if(IS_KATAKANA(c)) 
  {
      return kWbClassKatakanaLetter;
  } 
  else if(IS_HIRAGANA(c)) 
  {
      return kWbClassHiraganaLetter;
  } 
  else 
  {
      return kWbClassAlphaLetter;
  }

  return 0;
}

nsresult nsSampleWordBreaker::PostionToBoundary(
  const PRUnichar* aText , PRUint32 aTextLen,
  PRUint32 aOffset,
  PRUint32 *oWordBegin,
  PRUint32 *oWordEnd)
{
  NS_PRECONDITION( nsnull != aText, "null ptr");
  NS_PRECONDITION( 0 != aTextLen, "len = 0");
  NS_PRECONDITION( nsnull != oWordBegin, "null ptr");
  NS_PRECONDITION( nsnull != oWordEnd, "null ptr");
  NS_PRECONDITION( aOffset <= aTextLen, "aOffset > aTextLen");

  if((nsnull == aText ) || (nsnull == oWordBegin) || (nsnull == oWordEnd))
    return NS_ERROR_NULL_POINTER; 
  
  if( aOffset > aTextLen )
    return NS_ERROR_ILLEGAL_VALUE;


  PRUint8 c = this->GetClass(aText[aOffset]);
  PRUint32 i;
  // Scan forward
  for(i = aOffset +1;i <= aTextLen; i++)
  {
     if( c != this->GetClass(aText[i]))
     {
       *oWordEnd = i;
       break;
     }
  }

  // Scan backward
  for(i = aOffset -1;i >= 0; i++)
  {
     if( c != this->GetClass(aText[i]))
     {
       *oWordBegin = i;
       break;
     }
  }

  return NS_OK;
}

nsresult nsSampleWordBreaker::FirstForwardBreak   (nsIBreakState* state)
{
  NS_PRECONDITION( nsnull != state, "null ptr");
  if(nsnull == state ) 
    return NS_ERROR_NULL_POINTER; 

  nsresult res;

  PRUint32 len;
  res = state->Length(&len);

  if(len < 2)
  {
    res = state->Set(len, PR_TRUE);
    return NS_OK;
  }

  const PRUnichar* text;
  res = state->GetText(&text);

  PRUint32 cur;
  res = state->Current(&cur);

  PRUint32 next = Next(text, len, 0);
  res = state->Set(next , (next == len) );

  return NS_OK;
}
nsresult nsSampleWordBreaker::NextForwardBreak    (nsIBreakState* state)
{
  NS_PRECONDITION( nsnull != state, "null ptr");
  if(nsnull == state ) 
    return NS_ERROR_NULL_POINTER; 


  PRBool done;
  nsresult res;
  res = state->IsDone(&done);
  if(done)
    return NS_OK;

  const PRUnichar* text;
  res = state->GetText(&text);

  PRUint32 len;
  res = state->Length(&len);

  PRUint32 cur;
  res = state->Current(&cur);


  PRUint32 next = Next(text, len, cur);
  res = state->Set(next , (next == len) );
  return NS_OK;
}

PRUint32 nsSampleWordBreaker::Next(
  const PRUnichar* aText,
  PRUint32 aLen,
  PRUint32 aPos
)
{
  PRInt8 c1, c2;
  PRUint32 cur = aPos;
  c1 = this->GetClass(aText[cur]);

  for(cur++; cur <aLen; cur++)
  {
     c2 = this->GetClass(aText[cur]);
     if(c2 != c1) 
       break;
  }
  return cur;
}

