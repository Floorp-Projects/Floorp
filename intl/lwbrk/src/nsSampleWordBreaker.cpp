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


#include "nsSampleWordBreaker.h"

#include "pratom.h"
#include "nsLWBRKDll.h"
nsSampleWordBreaker::nsSampleWordBreaker()
{
  NS_INIT_REFCNT();
}
nsSampleWordBreaker::~nsSampleWordBreaker()
{
}

NS_IMPL_ISUPPORTS1(nsSampleWordBreaker, nsIWordBreaker);

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
  kWbClassHiraganaLetter,
  kWbClassHWKatakanaLetter,
  kWbClassThaiLetter
} wb_class;

#define IS_ASCII(c)            (0 == ( 0xFF80 & (c)))
#define ASCII_IS_ALPHA(c)         ((( 'a' <= (c)) && ((c) <= 'z')) || (( 'A' <= (c)) && ((c) <= 'Z')))
#define ASCII_IS_DIGIT(c)         (( '0' <= (c)) && ((c) <= '9'))
#define ASCII_IS_SPACE(c)         (( ' ' == (c)) || ( '\t' == (c)) || ( '\r' == (c)) || ( '\n' == (c)))
#define IS_ALPHABETICAL_SCRIPT(c) ((c) < 0x2E80) 

// we change the beginning of IS_HAN from 0x4e00 to 0x3400 to relfect Unicode 3.0 
#define IS_HAN(c)              (( 0x3400 <= (c)) && ((c) <= 0x9fff))||(( 0xf900 <= (c)) && ((c) <= 0xfaff))
#define IS_KATAKANA(c)         (( 0x30A0 <= (c)) && ((c) <= 0x30FF))
#define IS_HIRAGANA(c)         (( 0x3040 <= (c)) && ((c) <= 0x309F))
#define IS_HALFWIDTHKATAKANA(c)         (( 0xFF60 <= (c)) && ((c) <= 0xFF9F))
#define IS_THAI(c)         (0x0E00 == (0xFF80 & (c) )) // Look at the higest 9 bits

PRUint8 nsSampleWordBreaker::GetClass(PRUnichar c)
{
  // begin of the hack

  if (IS_ALPHABETICAL_SCRIPT(c))  {
	  if(IS_ASCII(c))  {
		  if(ASCII_IS_SPACE(c)) {
			  return kWbClassSpace;
		  } else if(ASCII_IS_ALPHA(c) || ASCII_IS_DIGIT(c)) {
			  return kWbClassAlphaLetter;
		  } else {
			  return kWbClassPunct;
		  }
	  } else if(IS_THAI(c))	{
		  return kWbClassThaiLetter;
	  } else {
		  return kWbClassAlphaLetter;
	  }
  }  else {
	  if(IS_HAN(c)) {
		  return kWbClassHanLetter;
	  } else if(IS_KATAKANA(c))   {
		  return kWbClassKatakanaLetter;
	  } else if(IS_HIRAGANA(c))   {
		  return kWbClassHiraganaLetter;
	  } else if(IS_HALFWIDTHKATAKANA(c))  {
		  return kWbClassHWKatakanaLetter;
	  } else  {
		  return kWbClassAlphaLetter;
	  }
  }
  return 0;
}

nsresult nsSampleWordBreaker::FindWord(
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
  *oWordEnd = aTextLen;
  for(i = aOffset +1;i <= aTextLen; i++)
  {
     if( c != this->GetClass(aText[i]))
     {
       *oWordEnd = i;
       break;
     }
  }

  // Scan backward
  *oWordBegin = 0;
  for(i = aOffset ;i > 0; i--)
  {
     if( c != this->GetClass(aText[i-1]))
     {
       *oWordBegin = i;
       break;
     }
  }
  if(kWbClassThaiLetter == c)
  {
	// need to call Thai word breaker from here
	// we should pass the whole Thai segment to the thai word breaker to find a shorter answer
  }
  return NS_OK;
}

nsresult nsSampleWordBreaker::Next( 
  const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos,
  PRUint32* oNext, PRBool *oNeedMoreText) 
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
  if(kWbClassThaiLetter == c1)
  {
	// need to call Thai word breaker from here
	// we should pass the whole Thai segment to the thai word breaker to find a shorter answer
  }
  *oNext = cur;
  *oNeedMoreText = (cur == aLen) ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

nsresult nsSampleWordBreaker::Prev( const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos,
  PRUint32* oPrev, PRBool *oNeedMoreText) 
{
  PRInt8 c1, c2;
  PRUint32 cur = aPos;
  c1 = this->GetClass(aText[cur]);

  for(; cur > 0; cur--)
  {
     c2 = this->GetClass(aText[cur-1]);
     if(c2 != c1)
       break;
  }
  if(kWbClassThaiLetter == c1)
  {
	// need to call Thai word breaker from here
	// we should pass the whole Thai segment to the thai word breaker to find a shorter answer
  }
  *oPrev = cur;
  *oNeedMoreText = (cur == 0) ? PR_TRUE : PR_FALSE;
  return NS_OK;
}
