/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsUCSupport.h"
#include "nsUTF8ToUnicode.h"

#define UNICODE_BYTE_ORDER_MARK    0xFEFF

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

  mState = 0;     // cached expected number of octets after the current octet
                  // until the beginning of the next UTF8 character sequence
  mUcs4  = 0;     // cached Unicode character
  mBytes = 1;     // cached expected number of octets in the current sequence
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

  nsresult res = NS_OK; // conversion result

  for (in = aSrc, out = aDest; ((in < inend) && (out < outend)); ++in) {
    if (0 == mState) {
      // When mState is zero we expect either a US-ASCII character or a
      // multi-octet sequence.
      if (0 == (0x80 & (*in))) {
        // US-ASCII, pass straight through.
        *out++ = (PRUnichar)*in;
        mBytes = 1;
      } else if (0xC0 == (0xE0 & (*in))) {
        // First octet of 2 octet sequence
        mUcs4 = (PRUint32)(*in);
        mUcs4 = (mUcs4 & 0x1F) << 6;
        mState = 1;
        mBytes = 2;
      } else if (0xE0 == (0xF0 & (*in))) {
        // First octet of 3 octet sequence
        mUcs4 = (PRUint32)(*in);
        mUcs4 = (mUcs4 & 0x0F) << 12;
        mState = 2;
        mBytes = 3;
      } else if (0xF0 == (0xF8 & (*in))) {
        // First octet of 4 octet sequence
        mUcs4 = (PRUint32)(*in);
        mUcs4 = (mUcs4 & 0x07) << 18;
        mState = 3;
        mBytes = 4;
      } else if (0xF8 == (0xFC & (*in))) {
        /* First octet of 5 octet sequence.
         *
         * This is illegal because the encoded codepoint must be either
         * (a) not the shortest form or
         * (b) outside the Unicode range of 0-0x10FFFF.
         * Rather than trying to resynchronize, we will carry on until the end
         * of the sequence and let the later error handling code catch it.
         */
        mUcs4 = (PRUint32)(*in);
        mUcs4 = (mUcs4 & 0x03) << 24;
        mState = 4;
        mBytes = 5;
      } else if (0xFC == (0xFE & (*in))) {
        // First octet of 6 octet sequence, see comments for 5 octet sequence.
        mUcs4 = (PRUint32)(*in);
        mUcs4 = (mUcs4 & 1) << 30;
        mState = 5;
        mBytes = 6;
      } else {
        /* Current octet is neither in the US-ASCII range nor a legal first
         * octet of a multi-octet sequence.
         *
         * Return an error condition. Caller is responsible for flushing and
         * refilling the buffer and resetting state.
         */
        res = NS_ERROR_UNEXPECTED;
        break;
      }
    } else {
      // When mState is non-zero, we expect a continuation of the multi-octet
      // sequence
      if (0x80 == (0xC0 & (*in))) {
        // Legal continuation.
        PRUint32 shift = (mState - 1) * 6;
        PRUint32 tmp = *in;
        tmp = (tmp & 0x0000003FL) << shift;
        mUcs4 |= tmp;

        if (0 == --mState) {
          /* End of the multi-octet sequence. mUcs4 now contains the final
           * Unicode codepoint to be output
           *
           * Check for illegal sequences and codepoints.
           */

          // From Unicode 3.1, non-shortest form is illegal
          if (((2 == mBytes) && (mUcs4 < 0x0080)) ||
              ((3 == mBytes) && (mUcs4 < 0x0800)) ||
              ((4 == mBytes) && (mUcs4 < 0x10000)) ||
              (4 < mBytes) ||
              // From Unicode 3.2, surrogate characters are illegal
              ((mUcs4 & 0xFFFFF800) == 0xD800) ||
              // Codepoints outside the Unicode range are illegal
              (mUcs4 > 0x10FFFF)) {
            res = NS_ERROR_UNEXPECTED;
            break;
          }
          if (mUcs4 > 0xFFFF) {
            // mUcs4 is in the range 0x10000 - 0x10FFFF. Output a UTF-16 pair
            mUcs4 -= 0x00010000;
            *out++ = 0xD800 | (0x000003FF & (mUcs4 >> 10));
            *out++ = 0xDC00 | (0x000003FF & mUcs4);
          } else if (UNICODE_BYTE_ORDER_MARK != mUcs4) {
            // BOM is legal but we don't want to output it
            *out++ = mUcs4;
          }
          //initialize UTF8 cache
          Reset();
        }
      } else {
        /* ((0xC0 & (*in) != 0x80) && (mState != 0))
         * 
         * Incomplete multi-octet sequence. Unconsume this
         * octet and return an error condition. Caller is responsible
         * for flushing and refilling the buffer and resetting state.
         */
        in--;
        res = NS_ERROR_UNEXPECTED;
        break;
      }
    }
  }

  // output not finished, output buffer too short
  if ((NS_OK == res) && (in < inend) && (out >= outend))
    res = NS_OK_UDEC_MOREOUTPUT;

  // last UCS4 is incomplete, make sure the caller
  // returns with properly aligned continuation of the buffer
  if ((NS_OK == res) && (mState != 0))
    res = NS_OK_UDEC_MOREINPUT;

  *aSrcLength = in - aSrc;
  *aDestLength = out - aDest;

  return(res);
}
