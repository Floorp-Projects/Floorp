/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */



#include "nsJISx4501LineBreaker.h"



#include "pratom.h"
#include "nsLWBRKDll.h"
#include "jisx4501class.h"
#define TH_UNICODE
#include "th_char.h"
#include "rulebrk.h"


/* 

   Simplification of Pair Table in JIS X 4501

   1. The Origion Table - in 4.1.3

   In JIS x 4501. The pair table is defined as below

   Class of
   Leading    Class of Trialing Char Class
   Char        

              1  2  3  4  5  6  7  8  9 10 11 12 13 13 14 14 15 16 17 18 19 20
                                                 *  #  *  #
        1     X  X  X  X  X  X  X  X  X  X  X  X  X  X  X  X  X  X  X  X  X  E
        2        X  X  X  X  X                                               X
        3        X  X  X  X  X                                               X
        4        X  X  X  X  X                                               X
        5        X  X  X  X  X                                               X
        6        X  X  X  X  X                                               X
        7        X  X  X  X  X  X                                            X 
        8        X  X  X  X  X                                X              E 
        9        X  X  X  X  X                                               X
       10        X  X  X  X  X                                               X
       11        X  X  X  X  X                                               X
       12        X  X  X  X  X                                               X  
       13        X  X  X  X  X                    X                          X
       14        X  X  X  X  X                          X                    X
       15        X  X  X  X  X        X                       X        X     X 
       16        X  X  X  X  X                                   X     X     X
       17        X  X  X  X  X                                               E 
       18        X  X  X  X  X                                X  X     X     X 
       19     X  E  E  E  E  E  X  X  X  X  X  X  X  X  X  X  X  X  E  X  E  E
       20        X  X  X  X  X                                               E

   * Same Char
   # Other Char
    
   X Cannot Break

   2. Simplified by remove the class which we do not care

   However, since we do not care about class 13(Subscript), 14(Ruby), 
   19(split line note begin quote), and 20(split line note end quote) 
   we can simplify this par table into the following 

   Class of
   Leading    Class of Trialing Char Class
   Char        

              1  2  3  4  5  6  7  8  9 10 11 12 15 16 17 18 
                                                 
        1     X  X  X  X  X  X  X  X  X  X  X  X  X  X  X  X
        2        X  X  X  X  X                             
        3        X  X  X  X  X                            
        4        X  X  X  X  X                           
        5        X  X  X  X  X                          
        6        X  X  X  X  X                         
        7        X  X  X  X  X  X                      
        8        X  X  X  X  X                    X    
        9        X  X  X  X  X                                   
       10        X  X  X  X  X                                  
       11        X  X  X  X  X                                 
       12        X  X  X  X  X                                
       15        X  X  X  X  X        X           X        X    
       16        X  X  X  X  X                       X     X    
       17        X  X  X  X  X                                  
       18        X  X  X  X  X                    X  X     X    

   3. Simplified by merged classes

   After the 2 simplification, the pair table have some duplication 
   a. class 2, 3, 4, 5, 6,  are the same- we can merged them
   b. class 10, 11, 12, 17  are the same- we can merged them


   Class of
   Leading    Class of Trialing Char Class
   Char        

              1 [a] 7  8  9 [b]15 16 18 
                                     
        1     X  X  X  X  X  X  X  X  X
      [a]        X                             
        7        X  X                      
        8        X              X    
        9        X                                   
      [b]        X                                  
       15        X        X     X     X    
       16        X                 X  X    
       18        X              X  X  X    



   4. We add THAI characters and make it breakable w/ all ther class

   Class of
   Leading    Class of Trialing Char Class
   Char        

              1 [a] 7  8  9 [b]15 16 18 THAI
                                     
        1     X  X  X  X  X  X  X  X  X
      [a]        X                             
        7        X  X                      
        8        X              X    
        9        X                                   
      [b]        X                                  
       15        X        X     X     X    
       16        X                 X  X    
       18        X              X  X  X    
     THAI                                T
      
     T : need special handling

   5. Now we use one bit to encode weather it is breakable, and use 2 bytes
      for one row, then the bit table will look like:

                 18    <-   1
            
       1  0000 0001 1111 1111  = 0x01FF
      [a] 0000 0000 0000 0010  = 0x0002
       7  0000 0000 0000 0110  = 0x0006
       8  0000 0000 0100 0010  = 0x0042
       9  0000 0000 0000 0010  = 0x0002
      [b] 0000 0000 0000 0010  = 0x0002
      15  0000 0001 0101 0010  = 0x0152
      16  0000 0001 1000 0010  = 0x0182
      18  0000 0001 1100 0010  = 0x01C2
    THAI  0000 0000 0000 0000  = 0x0000

   5. Now we map the class to number
      
      0: 1 
      1: [a]- 2, 3, 4, 5, 6
      2: 7
      3: 8
      4: 9
      5: [b]- 10, 11, 12, 17
      6: 15
      7: 16
      8: 18
      9: THAI

*/

#define MAX_CLASSES 10

static PRUint16 gPair[MAX_CLASSES] = {
  0x01FF, 
  0x0002, 
  0x0006, 
  0x0042, 
  0x0002, 
  0x0002, 
  0x0152, 
  0x0182, 
  0x01C2,
  0x0000
};


static inline int
GETCLASSFROMTABLE(PRUint32* t, PRUint16 l)
{
  return ((((t)[(l>>3)]) >> ((l & 0x0007)<<2)) & 0x000f);
}

#define CLASS_THAI 9



static inline int
IS_HALFWIDTH_IN_JISx4501_CLASS3(PRUnichar u)
{
  return ((0xff66 <= (u)) && ((u) <= 0xff70));
}

static inline int
IS_CJK_CHAR(PRUnichar u)
{
  return ((0x1100 <= (u) && (u) <= 0x11ff) ||
          (0x2e80 <= (u) && (u) <= 0xd7ff) ||
          (0xf900 <= (u) && (u) <= 0xfaff) ||
          (0xff00 <= (u) && (u) <= 0xffff) );
}

static inline int
IS_SPACE(PRUnichar u)
{
  return ((u) == 0x0020 || (u) == 0x0009 || (u) == 0x000a || (u) == 0x000d || (u)==0x200b);
}

PRInt8 nsJISx4501LineBreaker::GetClass(PRUnichar u)
{
   PRUint16 h = u & 0xFF00;
   PRUint16 l = u & 0x00ff;
   PRInt8 c;
   
   // Handle 3 range table first
   if( 0x0000 == h)
   {
     c = GETCLASSFROMTABLE(gLBClass00, l);
   } 
   else if(th_isthai(u))
   {
     c = CLASS_THAI;
   }
   else if( 0x2000 == h)
   {
     c = GETCLASSFROMTABLE(gLBClass20, l);
   } 
   else if( 0x2100 == h)
   {
     c = GETCLASSFROMTABLE(gLBClass21, l);
   } 
   else if( 0x3000 == h)
   {
     c = GETCLASSFROMTABLE(gLBClass30, l);
   } else if (( ( 0x3200 <= h) && ( h <= 0x9fff) ) || // Unicode 3.0
              ( ( 0xAC00 <= h) && ( h <= 0xD7FF) ) || // Hangul
              ( ( 0xf900 <= h) && ( h <= 0xfaff) ) 
             )
   { 
     c = 5; // CJK charcter, Han, and Han Compatability
   } else if( 0xff00 == h)
   {
     if( l < 0x0060) // Fullwidth ASCII variant 
     {
       c = GETCLASSFROMTABLE(gLBClass00, (l+0x20));
     } else if (l < 0x00a0) {
       switch (l)
       {
         case 0x61: c = GetClass(0x3002); break;
         case 0x62: c = GetClass(0x300c); break;
         case 0x63: c = GetClass(0x300d); break;
         case 0x64: c = GetClass(0x3001); break;
         case 0x65: c = GetClass(0x30fb); break;
         case 0x9e: c = GetClass(0x309b); break;
         case 0x9f: c = GetClass(0x309c); break;
         default:
           if(IS_HALFWIDTH_IN_JISx4501_CLASS3(u))
              c = 1; // jis x4501 class 3
           else
              c = 5; // jis x4501 class 11
           break;
       };
       // Halfwidth Katakana variants
     } else if( l < 0x00e0) {
       c = 8; // Halfwidth Hangul variants 
     } else if( l < 0x00f0) {
		 static PRUnichar NarrowFFEx[16] = 
		 { 
			 0x00A2, 0x00A3, 0x00AC, 0x00AF, 0x00A6, 0x00A5, 0x20A9, 0x0000,
		     0x2502, 0x2190, 0x2191, 0x2192, 0x2193, 0x25A0, 0x25CB, 0x0000
		 };
		 c = GetClass(NarrowFFEx[l - 0x00e0]);
	 } else {
		 c = 8;
	 };
   } else {
     c = 8; // others 
   }
   return c;
}

PRBool nsJISx4501LineBreaker::GetPair(PRInt8 c1, PRInt8 c2)
{
  NS_ASSERTION( c1 < MAX_CLASSES ,"illegal classes 1");
  NS_ASSERTION( c2 < MAX_CLASSES ,"illegal classes 2");

  return (0 == ((gPair[c1] >> c2 ) & 0x0001));
}


nsJISx4501LineBreaker::nsJISx4501LineBreaker(
   const PRUnichar* aNoBegin, PRInt32 aNoBeginLen,
   const PRUnichar* aNoEnd, PRInt32 aNoEndLen
)
{
  NS_INIT_REFCNT();
}
nsJISx4501LineBreaker::~nsJISx4501LineBreaker()
{
}

NS_IMPL_ISUPPORTS1(nsJISx4501LineBreaker, nsILineBreaker);

#define U_PERIOD ((PRUnichar) '.')
#define U_COMMA ((PRUnichar) ',')
#define U_SPACE ((PRUnichar) ' ')
#define U_RIGHT_SINGLE_QUOTATION_MARK ((PRUnichar) 0x2019)
#define NEED_CONTEXTUAL_ANALYSIS(c) (((c)==U_PERIOD)||((c)==U_COMMA)||((c)==U_RIGHT_SINGLE_QUOTATION_MARK))
#define NUMERIC_CLASS  6 // JIS x4501 class 15 is now map to simplified class 6
#define CHARACTER_CLASS  8 // JIS x4501 class 18 is now map to simplified class 8
#define IS_ASCII_DIGIT(u) ((0x0030 <= (u)) && ((u) <= 0x0039))

PRInt8  nsJISx4501LineBreaker::ContextualAnalysis(
   PRUnichar prev, PRUnichar cur, PRUnichar next
)
{
   if(  U_COMMA == cur)
   {
         if( IS_ASCII_DIGIT (prev)  && 
             IS_ASCII_DIGIT (next) )
           return NUMERIC_CLASS;
   }
   else if( U_PERIOD == cur)
   {
         if( (IS_ASCII_DIGIT (prev) || (0x0020 == prev) )&& 
             IS_ASCII_DIGIT (next) )
           return NUMERIC_CLASS;
         if( U_SPACE != next )
           return CHARACTER_CLASS;
   }
   else if(  U_RIGHT_SINGLE_QUOTATION_MARK == cur)
   {
        // somehow people use this as ' in "it's" sometimes...
        if( U_SPACE != next )
           return CHARACTER_CLASS;
   }
   return this->GetClass(cur);
}

NS_IMETHODIMP nsJISx4501LineBreaker::BreakInBetween(
  const PRUnichar* aText1 , PRUint32 aTextLen1,
  const PRUnichar* aText2 , PRUint32 aTextLen2,
  PRBool *oCanBreak)
{
  NS_PRECONDITION( nsnull != aText1, "null ptr");
  NS_PRECONDITION( nsnull != aText2, "null ptr");
  if((nsnull == aText1) || (nsnull == aText2 ))
     return NS_ERROR_NULL_POINTER;

  if((0 == aTextLen1) || (0==aTextLen2))
  {
     *oCanBreak = PR_FALSE;
     return NS_OK;
  }

  //search for CJK characters until a space is found. 
  //if CJK char is found before space, use 4051, otherwise western
  PRInt32 cur;

  for (cur= aTextLen1-1; cur>=0; cur--)
  {
	  if (IS_SPACE(aText1[cur]))
		  break;
	  if (IS_CJK_CHAR(aText1[cur]))
		  goto ROUTE_CJK_BETWEEN;
  }

  for (cur= 0; cur < (PRInt32)aTextLen2; cur++)
  {
	  if (IS_SPACE(aText2[cur]))
		  break;
	  if (IS_CJK_CHAR(aText2[cur]))
		  goto ROUTE_CJK_BETWEEN;
  }

  //now apply western rule.
  *oCanBreak = (IS_SPACE(aText1[aTextLen1-1]) || IS_SPACE(aText2[0]));
  return NS_OK;

ROUTE_CJK_BETWEEN:

  PRInt8 c1, c2;
  if(NEED_CONTEXTUAL_ANALYSIS(aText1[aTextLen1-1]))
    c1 = this->ContextualAnalysis((aTextLen1>1)?aText1[aTextLen1-2]:0,
                                  aText1[aTextLen1-1],
                                  aText2[0]);
  else 
    c1 = this->GetClass(aText1[aTextLen1-1]);

  if(NEED_CONTEXTUAL_ANALYSIS(aText2[0]))
    c2 = this->ContextualAnalysis(aText1[aTextLen1-1],
                                  aText2[0],
                                  (aTextLen2>1)?aText2[1]:0);
  else 
    c2 = this->GetClass(aText2[0]);

  /* Handle cases for THAI */
  if((CLASS_THAI == c1) && (CLASS_THAI == c2))
  {
     *oCanBreak = (0 == TrbWordBreakPos(aText1, aTextLen1, aText2, aTextLen2));
  }
  else 
  {
     *oCanBreak = GetPair(c1,c2);
  }
  return NS_OK;
}


NS_IMETHODIMP nsJISx4501LineBreaker::Next( 
  const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos,
  PRUint32* oNext, PRBool *oNeedMoreText) 
{
  NS_PRECONDITION( nsnull != aText, "null ptr");
  if(nsnull == aText)
     return NS_ERROR_NULL_POINTER;
  NS_PRECONDITION( nsnull != oNext, "null ptr");
  if(nsnull == oNext)
     return NS_ERROR_NULL_POINTER;
  NS_PRECONDITION( nsnull != oNeedMoreText, "null ptr");
  if(nsnull == oNeedMoreText)
     return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION( aPos <= aLen, "aPos > aLen");
  if( aPos > aLen )
     return NS_ERROR_ILLEGAL_VALUE;

  if((aPos + 1) > aLen)
  {
     *oNext = aLen;
     *oNeedMoreText = PR_TRUE;
     return NS_OK;
  }

  //forward check for CJK characters until a space is found. 
  //if CJK char is found before space, use 4051, otherwise western
  PRUint32 cur = aPos;

  for (cur= aPos; cur<(PRInt32)aLen; cur++)
  {
	  if (IS_SPACE(aText[cur]))
	  {
        *oNext = cur;
        *oNeedMoreText = PR_FALSE;
        return NS_OK;
	  }
	  if (IS_CJK_CHAR(aText[cur]))
		  goto ROUTE_CJK_NEXT;
  }
  *oNext = aLen;
  *oNeedMoreText = PR_TRUE;
  return NS_OK;

ROUTE_CJK_NEXT:
  PRInt8 c1, c2;
  cur = aPos;
  if(NEED_CONTEXTUAL_ANALYSIS(aText[cur]))
  {
    c1 = this->ContextualAnalysis((cur>0)?aText[cur-1]:0,
                                  aText[cur],
                                  (cur<(aLen-1)) ?aText[cur+1]:0);
  } else  {
    c1 = this->GetClass(aText[cur]);
  }
  
  if(CLASS_THAI == c1) 
  {
     *oNext = PRUint32(TrbFollowing(aText, aLen, aPos));
     *oNeedMoreText = PR_FALSE;
     return NS_OK;
  }

  for(cur++; cur <aLen; cur++)
  {
     if(NEED_CONTEXTUAL_ANALYSIS(aText[cur]))
     {
       c2= this->ContextualAnalysis((cur>0)?aText[cur-1]:0,
                                  aText[cur],
                                  (cur<(aLen-1)) ?aText[cur+1]:0);
     } else {
       c2 = this->GetClass(aText[cur]);
     }

     if(GetPair(c1, c2)) {
       *oNext = cur ;
       *oNeedMoreText = PR_FALSE;
       return NS_OK;
     }
     c1 = c2;
  }
  *oNext = aLen;
  *oNeedMoreText = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsJISx4501LineBreaker::Prev( 
  const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos,
  PRUint32* oPrev, PRBool *oNeedMoreText) 
{
  NS_PRECONDITION( nsnull != aText, "null ptr");
  if(nsnull == aText)
     return NS_ERROR_NULL_POINTER;
  NS_PRECONDITION( nsnull != oPrev, "null ptr");
  if(nsnull == oPrev)
     return NS_ERROR_NULL_POINTER;
  NS_PRECONDITION( nsnull != oNeedMoreText, "null ptr");
  if(nsnull == oNeedMoreText)
     return NS_ERROR_NULL_POINTER;

  if(aPos < 2)
  {
     *oPrev = 0;
     *oNeedMoreText = PR_TRUE;
     return NS_OK;
  }

  //backward check for CJK characters until a space is found. 
  //if CJK char is found before space, use 4051, otherwise western
  PRUint32 cur;

  cur = aPos-1;
  if (IS_SPACE(aText[cur]))
  {
        *oPrev = cur;
        *oNeedMoreText = PR_FALSE;
        return NS_OK;
  }
  else if (IS_CJK_CHAR(aText[cur]))
		  goto ROUTE_CJK_PREV;

  for (; cur>0; )
  {
	  cur--;
	  if (IS_SPACE(aText[cur]))
	  {
        *oPrev = cur+1;
        *oNeedMoreText = PR_FALSE;
        return NS_OK;
	  }
	  if (IS_CJK_CHAR(aText[cur]))
		  goto ROUTE_CJK_PREV;
  }

  *oPrev = 0;
  *oNeedMoreText = PR_TRUE;
  return NS_OK;

ROUTE_CJK_PREV:
  cur = aPos;
  PRInt8 c1, c2;
  if(NEED_CONTEXTUAL_ANALYSIS(aText[cur-1]))
  {
    c2 = this->ContextualAnalysis(((cur-1)>0)?aText[cur-2]:0,
                                  aText[cur-1],
                                  (cur<aLen) ?aText[cur]:0);
  } else  {
    c2 = this->GetClass(aText[cur-1]);
  }
  // To Do: 
  //
  // Should handle CLASS_THAI here
  //
  for(cur--; cur > 0; cur--)
  {
     if(NEED_CONTEXTUAL_ANALYSIS(aText[cur-1]))
     {
       c1= this->ContextualAnalysis(((cur-1)>0)?aText[cur-2]:0,
                                  aText[cur-1],
                                  (cur<aLen) ?aText[cur]:0);
     } else {
       c1 = this->GetClass(aText[cur-1]);
     }

     if(GetPair(c1, c2)) {
       *oPrev = cur;
       *oNeedMoreText = PR_FALSE;
       return NS_OK;
     }
     c2 = c1;
  }
  *oPrev = 0;
  *oNeedMoreText = PR_TRUE;
  return NS_OK;
}

