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

#ifndef nsIUnicodeDecoder_h___
#define nsIUnicodeDecoder_h___

#include "nscore.h"
#include "nsISupports.h"

// Interface ID for our Unicode Decoder interface
// {B2F178E1-832A-11d2-8A8E-00600811A836}
//NS_DECLARE_ID(kIUnicodeDecoderIID,
//  0xb2f178e1, 0x832a, 0x11d2, 0x8a, 0x8e, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

#define NS_IUNICODEDECODER_IID	\
	{ 0xb2f178e1, 0x832a, 0x11d2,	\
		{ 0x8a, 0x8e, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36 }}
static NS_DEFINE_IID(kIUnicodeDecoderIID, NS_IUNICODEDECODER_IID);

// XXX deprecated
/*---------- BEGIN DEPRECATED */ 
#define NS_EXACT_LENGTH \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_UCONV, 11)

#define NS_PARTIAL_MORE_INPUT \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_UCONV, 12)

#define NS_PARTIAL_MORE_OUTPUT \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_UCONV, 13)

#define NS_ERROR_ILLEGAL_INPUT \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_UCONV, 14)
/*---------- END DEPRECATED */ 

// XXX make us hex! (same digits though)
#define NS_OK_UDEC_EXACTLENGTH      \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_UCONV, 11)

#define NS_OK_UDEC_MOREINPUT        \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_UCONV, 12)

#define NS_OK_UDEC_MOREOUTPUT       \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_UCONV, 13)

#define NS_ERROR_UDEC_ILLEGALINPUT  \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_UCONV, 14)


#define NS_UNICODEDECODER_CONTRACTID_BASE "@mozilla.org/intl/unicode/decoder;1?charset="

/**
 * Interface for a Converter from a Charset into Unicode.
 *
 * @created         23/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsIUnicodeDecoder : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IUNICODEDECODER_IID)

  enum {
    kOnError_Recover,       // on an error, recover and continue
    kOnError_Signal         // on an error, stop and signal
  };

  /**
   * Converts the data from one Charset to Unicode.
   *
   * About the byte ordering:
   * - For input, if the converter cares (that depends of the charset, for 
   * example a singlebyte will ignore the byte ordering) it should assume 
   * network order. If necessary and requested, we can add a method 
   * SetInputByteOrder() so that the reverse order can be used, too. That 
   * method would have as default the assumed network order.
   * - The output stream is Unicode, having the byte order which is internal
   * for the machine on which the converter is running on.
   *
   * Unless there is not enough output space, this method must consume all the
   * available input data! The eventual incomplete final character data will be
   * stored internally in the converter and used when the method is called 
   * again for continuing the conversion. This way, the caller will not have to
   * worry about managing incomplete input data by mergeing it with the next 
   * buffer.
   *
   * Error conditions: 
   * If the read value does not belong to this character set, one should 
   * replace it with the Unicode special 0xFFFD. When an actual input error is 
   * encountered, like a format error, the converter stop and return error.
   * Hoever, we should keep in mind that we need to be lax in decoding.
   *
   * Converter required behavior:
   * In this order: when output space is full - return right away. When input
   * data is wrong, return input pointer right after the wrong byte. When 
   * partial input, it will be consumed and cached. All the time input pointer
   * will show how much was actually consumed and how much was actually 
   * written.
   *
   * @param aSrc        [IN] the source data buffer
   * @param aSrcLength  [IN/OUT] the length of source data buffer; after
   *                    conversion will contain the number of bytes read
   * @param aDest       [OUT] the destination data buffer
   * @param aDestLength [IN/OUT] the length of the destination data buffer;
   *                    after conversion will contain the number of Unicode
   *                    characters written
   * @return            NS_PARTIAL_MORE_INPUT if only a partial conversion was
   *                    done; more input is needed to continue
   *                    NS_PARTIAL_MORE_OUTPUT if only  a partial conversion
   *                    was done; more output space is needed to continue
   *                    NS_ERROR_ILLEGAL_INPUT if an illegal input sequence
   *                    was encountered and the behavior was set to "signal"
   */
  NS_IMETHOD Convert(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength) = 0;

  /**
   * Returns a quick estimation of the size of the buffer needed to hold the
   * converted data. Remember: this estimation is >= with the actual size of 
   * the buffer needed. It will be computed for the "worst case"
   *
   * @param aSrc        [IN] the source data buffer
   * @param aSrcLength  [IN] the length of source data buffer
   * @param aDestLength [OUT] the needed size of the destination buffer
   * @return            NS_EXACT_LENGTH if an exact length was computed
   *                    NS_OK is all we have is an approximation
   */
  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength) = 0;

  /**
   * Resets the charset converter so it may be recycled for a completely 
   * different and urelated buffer of data.
   */
  NS_IMETHOD Reset() = 0;
};

#endif /* nsIUnicodeDecoder_h___ */
