/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Jungshik Shin <jshin@mailaps.org>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsISO2022KRToUnicode.h"
#include "nsUCvKOSupport.h"
#include "nsICharsetConverterManager.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

NS_IMETHODIMP nsISO2022KRToUnicode::Convert(const char * aSrc, PRInt32 * aSrcLen, PRUnichar * aDest, PRInt32 * aDestLen)
{
  const unsigned char* srcEnd = (unsigned char*)aSrc + *aSrcLen;
  const unsigned char* src =(unsigned char*) aSrc;
  PRUnichar* destEnd = aDest + *aDestLen;
  PRUnichar* dest = aDest;
  while((src < srcEnd))
  {
    switch(mState)
    {
      case mState_ASCII:
        if(0x1b == *src) {
            mLastLegalState = mState;
            mState = mState_ESC;
        } 
        else if(0x0e == *src) { // Shift-Out 
          mState = mState_KSX1001_1992;
        } 
        else if(*src & 0x80) {
          *dest++ = 0xFFFD;
          if(dest >= destEnd)
            goto error1;
        } 
        else {
          *dest++ = (PRUnichar) *src;
          if(dest >= destEnd)
            goto error1;
        }
        break;
          
      case mState_ESC:
        if('$' == *src) {
          mState = mState_ESC_24;
        } 
        else  {
          if((dest+2) >= destEnd)
            goto error1;
          *dest++ = (PRUnichar) 0x1b;
          *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
          mState =  mLastLegalState;
        }
        break;

      case mState_ESC_24: // ESC $
        if(')' == *src) {
          mState = mState_ESC_24_29;
        } 
        else  {
          if((dest+3) >= destEnd)
            goto error1;
          *dest++ = (PRUnichar) 0x1b;
          *dest++ = (PRUnichar) '$';
          *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
          mState = mLastLegalState;
        }
        break;

      case mState_ESC_24_29: // ESC $ )
        mState = mLastLegalState;
        if('C' == *src) {
          mState = mState_ASCII;
        } 
        else  {
          if((dest+4) >= destEnd)
            goto error1;
          *dest++ = (PRUnichar) 0x1b;
          *dest++ = (PRUnichar) '$';
          *dest++ = (PRUnichar) ')';
          *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
          mState = mLastLegalState;
        }
        break;

      case mState_KSX1001_1992:
        if (0x20 < (PRUint8) *src  && (PRUint8) *src < 0x7f) {
          mData = (PRUint8) *src;
          mState = mState_KSX1001_1992_2ndbyte;
        } 
        else if (0x0f == *src) { // Shift-In (SI)
          mState = mState_ASCII;
        } 
        else if ((PRUint8) *src == 0x20 || (PRUint8) *src == 0x09) {
          // Allow space and tab between SO and SI (i.e. in Hangul segment)
          mState = mState_KSX1001_1992;
          *dest++ = (PRUnichar) *src;
          if(dest >= destEnd)
          goto error1;
        } 
        else {         // Everything else is invalid.
          *dest++ = 0xFFFD;
          if(dest >= destEnd)
             goto error1;
        }
        break;

      case mState_KSX1001_1992_2ndbyte:
        if ( 0x20 < (PRUint8) *src && (PRUint8) *src < 0x7f  ) {
          if (!mEUCKRDecoder) {
            // creating a delegate converter (EUC-KR)
            nsresult rv;
            nsString tmpCharset;
            nsCOMPtr<nsICharsetConverterManager> ccm = 
                  do_GetService(kCharsetConverterManagerCID, &rv);
            if (!NS_FAILED(rv)) {
              tmpCharset.AssignWithConversion("EUC-KR");
              rv = ccm->GetUnicodeDecoder(&tmpCharset, &mEUCKRDecoder);
            }
          }

          if (!mEUCKRDecoder) {// failed creating a delegate converter
           *dest++ = 0xFFFD;
          } 
          else {              
            unsigned char ksx[2];
            PRUnichar uni;
            PRInt32 ksxLen = 2, uniLen = 1;
            // mData is the original 1st byte.
            // *src is the present 2nd byte.
            // Put 2 bytes (one character) to ksx[] with EUC-KR encoding.
            ksx[0] = mData | 0x80;
            ksx[1] = *src | 0x80;
            // Convert EUC-KR to unicode.
            mEUCKRDecoder->Convert((const char *)ksx, &ksxLen, &uni, &uniLen);
            *dest++ = uni;
          }
          if(dest >= destEnd)
            goto error1;
          mState = mState_KSX1001_1992;
        } 
        else {        // Invalid 
          if ( 0x0f == *src ) {   // Shift-In (SI)
            mState = mState_ASCII;
          } 
          else {
            mState = mState_KSX1001_1992;
          }
          *dest++ = 0xFFFD;
          if(dest >= destEnd)
           goto error1;
        }
        break;

      case mState_ERROR:
        mState = mLastLegalState;
        *dest++ = 0xFFFD;
        if(dest >= destEnd)
          goto error1;
        break;

    } // switch
    src++;
    if ( *src == 0x0a || *src == 0x0d )   // if LF/CR, return to US-ASCII unconditionally.
      mState = mState_ASCII;
   }
   *aDestLen = dest - aDest;
   return NS_OK;

error1:
   *aDestLen = dest-aDest;
   *aSrcLen = src-(unsigned char*)aSrc;
   return NS_OK_UDEC_MOREOUTPUT;
}

