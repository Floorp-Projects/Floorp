/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsITextTransform.h"
#include "pratom.h"
#include "nsUUDll.h"
#include "nsHankakuToZenkakuCID.h"
#include "nsTextTransformFactory.h"

NS_DEFINE_CID(kHankakuToZenkakuCID, NS_HANKAKUTOZENKAKU_CID);

// Basic mapping from Hankaku to Zenkaku
// Nigori and Maru is take care out side this basic mapping
static PRUnichar gBasicMapping[0x40] =
{
// 0xff60
0xff60,0x3002,0x300c,0x300d,0x3001,0x30fb,0x30f2,0x30a1,        
// 0xff68
0x30a3,0x30a5,0x30a7,0x30a9,0x30e3,0x30e5,0x30e7,0x30c3,        
// 0xff70
0x30fc,0x30a2,0x30a4,0x30a6,0x30a8,0x30aa,0x30ab,0x30ad,        
// 0xff78
0x30af,0x30b1,0x30b3,0x30b5,0x30b7,0x30b9,0x30bb,0x30bd,        
// 0xff80
0x30bf,0x30c1,0x30c4,0x30c6,0x30c8,0x30ca,0x30cb,0x30cc,        
// 0xff88
0x30cd,0x30ce,0x30cf,0x30d2,0x30d5,0x30d8,0x30db,0x30de,        
// 0xff90
0x30df,0x30e0,0x30e1,0x30e2,0x30e4,0x30e6,0x30e8,0x30e9,        
// 0xff98
0x30ea,0x30eb,0x30ec,0x30ed,0x30ef,0x30f3,0x309b,0x309c
};

// Do we need to check for Nigori for the next unicode ?
#define NEED_TO_CHECK_NIGORI(u) (((0xff76<=(u))&&((u)<=0xff84))||((0xff8a<=(u))&&((u)<=0xff8e)))

// Do we need to check for Maru for the next unicode ?
#define NEED_TO_CHECK_MARU(u) ((0xff8a<=(u))&&((u)<=0xff8e))

// The  unicode is in Katakana Hankaku block
#define IS_HANKAKU(u) (0xff60==((u) & 0xffe0)) || (0xff80==((u)&0xffe0))
#define IS_NIGORI(u) (0xff9e == (u))
#define IS_MARU(u)   (0xff9f == (u))
#define NIGORI_MODIFIER 1
#define MARU_MODIFIER   2
 
// function prototype 
void HankakuToZenkaku (
    const PRUnichar* aSrc, PRInt32 aLen, 
    PRUnichar* aDest, PRInt32 aDestLen, PRInt32* oLen);

void HankakuToZenkaku (
    const PRUnichar* aSrc, PRInt32 aLen, 
    PRUnichar* aDest, PRInt32 aDestLen, PRInt32* oLen)
{

    PRInt32 i,j;
    // loop from the first to the last char except the last one.
    for(i = j = 0; i < (aLen-1); i++,j++,aSrc++, aDest++)
    {
       if(IS_HANKAKU(*aSrc)) {
         // if it is in hankaku - do basic mapping first
         *aDest = gBasicMapping[(*aSrc) - 0xff60];
         
         // if is some char could be modifier, and the next char
         // is a modifier, modify it and eat one byte

         if(IS_NIGORI(*(aSrc+1)) && NEED_TO_CHECK_NIGORI(*aSrc))
         {
            *aDest += NIGORI_MODIFIER;
            i++; aSrc++;
         }
         else if(IS_MARU(*(aSrc+1)) && NEED_TO_CHECK_MARU(*aSrc)) 
         {
            *aDest += MARU_MODIFIER;
            i++; aSrc++;
         }
       }
       else 
       {
         // not in hankaku block, just copy 
         *aDest = *aSrc;
       }
    }

    // handle the last character
    if(IS_HANKAKU(*aSrc)) 
         *aDest = gBasicMapping[(*aSrc) - 0xff60];
    else
         *aDest = *aSrc;

    *oLen = j+1;
}



class nsHankakuToZenkaku : public nsITextTransform {
  NS_DECL_ISUPPORTS

public: 

  nsHankakuToZenkaku() ;
  virtual ~nsHankakuToZenkaku() ;
  NS_IMETHOD Change( nsString& aText, nsString& aResult);

};

NS_IMPL_ISUPPORTS1(nsHankakuToZenkaku, nsITextTransform)

nsHankakuToZenkaku::nsHankakuToZenkaku()
{
  NS_INIT_REFCNT();
}
nsHankakuToZenkaku::~nsHankakuToZenkaku()
{
}

NS_IMETHODIMP nsHankakuToZenkaku::Change( nsString& aText, nsString& aResult)
{
   aResult = aText;
   const PRUnichar* u = aResult.get();
   PRUnichar* ou = (PRUnichar*) u;
   PRInt32 l = aResult.Length();
   PRInt32 ol;
   
   HankakuToZenkaku ( u, l, ou, l, &ol);
   aResult.SetLength(ol);

   return NS_OK;
}

nsresult NS_NewHankakuToZenkaku(nsISupports** oResult)
{
  if(!oResult)
    return NS_ERROR_NULL_POINTER;
  *oResult = new nsHankakuToZenkaku();
  if(*oResult)
     NS_ADDREF(*oResult);
  return (*oResult) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
