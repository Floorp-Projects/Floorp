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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsUCvMinSupport.h"
#include "nsUTF8ToUnicode.h"

NS_IMETHODIMP NS_NewUTF8ToUnicode(nsISupports* aOuter, 
                                            const nsIID& aIID,
                                            void** aResult)
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_ERROR_NO_AGGREGATION;
  }
  nsUTF8ToUnicode * inst = new nsUTF8ToUnicode();
  if (!inst) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) {
    *aResult = nsnull;
    delete inst;
  }
  return res;
}

//----------------------------------------------------------------------
// Class nsUTF8ToUnicode [implementation]

nsUTF8ToUnicode::nsUTF8ToUnicode() 
: nsBasicDecoderSupport()

{
	Reset();
}

//----------------------------------------------------------------------
// Subclassing of nsTableDecoderSupport class [implementation]

NS_IMETHODIMP nsUTF8ToUnicode::GetMaxLength(const char * aSrc, 
                                            PRInt32 aSrcLength, 
                                            PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK;
}


//----------------------------------------------------------------------
// Subclassing of nsBasicDecoderSupport class [implementation]

 NS_IMETHODIMP nsUTF8ToUnicode::Reset()
{

	mState = 0;			// cached expected number of bytes per UTF8 character sequence
	mUcs4  = 0;			// cached Unicode character
  mBytes = 1;
	return NS_OK;

}

//----------------------------------------------------------------------
// Subclassing of nsBasicDecoderSupport class [implementation]

 
 NS_IMETHODIMP nsUTF8ToUnicode::Convert(const char * aSrc, 
                                                    PRInt32 * aSrcLength, 
                                                    PRUnichar * aDest, 
                                                    PRInt32 * aDestLength)
 {
   
   PRUint32 aSrcLen   = (PRUint32) (*aSrcLength);
   PRUint32 aDestLen = (PRUint32) (*aDestLength);
   
   const char *in, *inend;
   inend = aSrc + aSrcLen;
   
   PRUnichar *out, *outend;
   outend = aDest + aDestLen;

   nsresult res;	// conversion result

   for(in=aSrc,out=aDest,res=NS_OK;((in < inend) && (out < outend)); in++)
   {
      if(0 == mState) {
         if( 0 == (0x80 & (*in))) {
             // ASCII
             *out++ = (PRUnichar)*in;
        mBytes =1;
         } else if( 0xC0 == (0xE0 & (*in))) {
             // 2 bytes UTF8
             mUcs4 = (PRUint32)(*in);
             mUcs4 = (mUcs4 << 6) & 0x000007C0L;
             mState=1;
        mBytes =2;
         } else if( 0xE0 == (0xF0 & (*in))) {
			 // 3 bytes UTF8
             mUcs4 = (PRUint32)(*in);
             mUcs4 = (mUcs4 << 12) & 0x0000F000L;
             mState=2;
        mBytes =3;
         } else if( 0xF0 == (0xF8 & (*in))) {
			 // 4 bytes UTF8
             mUcs4 = (PRUint32)(*in);
             mUcs4 = (mUcs4 << 18) & 0x001F0000L;
             mState=3;
        mBytes =4;
         } else if( 0xF8 == (0xFC & (*in))) {
			 // 5 bytes UTF8
             mUcs4 = (PRUint32)(*in);
             mUcs4 = (mUcs4 << 24) & 0x03000000L;
             mState=4;
        mBytes =5;
         } else if( 0xFC == (0xFE & (*in))) {
			 // 6 bytes UTF8
             mUcs4 = (PRUint32)(*in);
             mUcs4 = (mUcs4 << 30) & 0x40000000L;
             mState=5;
        mBytes =6;
         } else {
			 //NS_ASSERTION(0, "The input string is not in utf8");
	  		 //unexpected octet, put in a replacement char, 
			 //flush and refill the buffer, reset state
			 res = NS_ERROR_UNEXPECTED;
			 break;
         }
	 } else {
      if(0x80 == (0xC0 & (*in))) {
             PRUint32 tmp = (*in);
        PRUint32 shift = (mState-1) * 6;
             tmp = (tmp << shift ) & ( 0x0000003FL << shift);
             mUcs4 |= tmp;
        if(0 == --mState) {
                 if(mUcs4 >= 0x00010000) {
                    if(mUcs4 >= 0x00110000) {
                      *out++ = 0xFFFD;
                    } else {
                      mUcs4 -= 0x00010000;
                      *out++ = 0xD800 | (0x000003FF & (mUcs4 >> 10));
                      *out++ = 0xDC00 | (0x000003FF & mUcs4);
                    }
                 } else {
            // from Unicode 3.1, non-shortest form is illegal 
            if(((2==mBytes) && (mUcs4 < 0x0080)) ||
               ((3==mBytes) && (mUcs4 < 0x0800)) ||
               ((4==mBytes) && (mUcs4 < 0x1000)) ||
                (5==mBytes) ||
                (6==mBytes)) 
            {
              res = NS_ERROR_UNEXPECTED;
              break;
            } 

                    if( 0xfeff != mUcs4 ) // ignore BOM
            {  
                      *out++ = mUcs4;
                 }
          }
				 //initialize UTF8 cache
				 Reset();
             }
         } else {
			 //NS_ASSERTION(0, "The input string is not in utf8");
	  		 //unexpected octet, put in a replacement char, 
			 //flush and refill the buffer, reset state
                         in--;
			 res = NS_ERROR_UNEXPECTED;
			 break;
         }
     }
   }

   //output not finished, output buffer too short
   if((NS_OK == res) && (in < inend) && (out >= outend)) 
       res = NS_OK_UDEC_MOREOUTPUT;

   //last USC4 is incomplete, make sure the caller 
   //returns with properly aligned continuation of the buffer
   if ((NS_OK == res) && (mState != 0))
       res = NS_OK_UDEC_MOREINPUT;

   *aSrcLength = in - aSrc;
   *aDestLength  = out - aDest;
   
   return(res);

 }
