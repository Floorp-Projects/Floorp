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



#include "pratom.h"
#include "nsLWBRKDll.h"
#include "jisx4501class.h"


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



   4. Now we use one bit to encode weather it is breakable, and use 2 bytes
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
*/

#define MAX_CLASSES 9

static PRUint16 gPair[MAX_CLASSES] = {
  0x01FF, 
  0x0002, 
  0x0006, 
  0x0042, 
  0x0002, 
  0x0002, 
  0x0152, 
  0x0182, 
  0x01C2
};


#define GETCLASSFROMTABLE(t, l) ((((t)[(l>>3)]) >> ((l & 0x0007)<<2)) & 0x000f)




#define IS_HALFWIDTH_IN_JISx4501_CLASS3(u) ((0xff66 <= (u)) && ((u) <= 0xff70))


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
   } else if (( ( 0x3200 <= h) && ( h <= 0x33ff) ) ||
              ( ( 0x4e00 <= h) && ( h <= 0x9fff) ) ||
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
     } else {
       c = 8; // Halfwidth Hangul variants 
     }
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
  PR_AtomicIncrement(&g_InstanceCount);
}
nsJISx4501LineBreaker::~nsJISx4501LineBreaker()
{
  PR_AtomicDecrement(&g_InstanceCount);
}

NS_DEFINE_IID(kILineBreakerIID, NS_ILINEBREAKER_IID);

NS_IMPL_ISUPPORTS(nsJISx4501LineBreaker, kILineBreakerIID);

#define U_PERIOD ((PRUnichar) '.')
#define U_COMMA ((PRUnichar) ',')
#define NEED_CONTEXTUAL_ANALYSIS(c) (((c)==U_PERIOD)||((c)==U_COMMA))
#define NUMERIC_CLASS  6 // JIS x4501 class 15 is now map to simplified class 6
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

  *oCanBreak = GetPair(c1,c2);
  return NS_OK;
}


NS_IMETHODIMP nsJISx4501LineBreaker::Next( 
  const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos,
  PRUint32* oNext, PRBool *oNeedMoreText) 
{
  NS_PRECONDITION( nsnull != aText, "null ptr");
  if(nsnull == aText)
     return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION( aPos < aLen, "aPos >= aLen");
  if( aPos > aLen )
     return NS_ERROR_ILLEGAL_VALUE;

  if((aPos + 1) > aLen)
  {
     *oNext = aLen;
     *oNeedMoreText = PR_TRUE;
     return NS_OK;
  }

  PRInt8 c1, c2;
  PRUint32 cur = aPos;
  if(NEED_CONTEXTUAL_ANALYSIS(aText[cur]))
  {
    c1 = this->ContextualAnalysis((cur>0)?aText[cur-1]:0,
                                  aText[cur],
                                  (cur<(aLen-1)) ?aText[cur+1]:0);
  } else  {
    c1 = this->GetClass(aText[cur]);
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

  if(aPos < 2)
  {
     *oPrev = 0;
     *oNeedMoreText = PR_TRUE;
     return NS_OK;
  }

  PRInt8 c1, c2;
  PRUint32 cur = aPos;
  if(NEED_CONTEXTUAL_ANALYSIS(aText[cur-1]))
  {
    c1 = this->ContextualAnalysis(((cur-1)>0)?aText[cur-2]:0,
                                  aText[cur-1],
                                  (cur<aLen) ?aText[cur]:0);
  } else  {
    c1 = this->GetClass(aText[cur-1]);
  }
  for(cur--; cur > 0; cur--)
  {
     if(NEED_CONTEXTUAL_ANALYSIS(aText[cur-1]))
     {
       c2= this->ContextualAnalysis(((cur-1)>0)?aText[cur-2]:0,
                                  aText[cur-1],
                                  (cur<aLen) ?aText[cur]:0);
     } else {
       c2 = this->GetClass(aText[cur-1]);
     }

     if(GetPair(c1, c2)) {
       *oPrev = cur - 1;
       *oNeedMoreText = PR_FALSE;
       return NS_OK;
     }
     c1 = c2;
  }
  *oPrev = 0;
  *oNeedMoreText = PR_TRUE;
  return NS_OK;
}

