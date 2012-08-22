/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIUnicodeDecoder_h___
#define nsIUnicodeDecoder_h___

#include "nscore.h"
#include "nsISupports.h"

// Interface ID for our Unicode Decoder interface
// {25359602-FC70-4d13-A9AB-8086D3827C0D}
//NS_DECLARE_ID(kIUnicodeDecoderIID,
//  0x25359602, 0xfc70, 0x4d13, 0xa9, 0xab, 0x80, 0x86, 0xd3, 0x82, 0x7c, 0xd);

#define NS_IUNICODEDECODER_IID	\
	{ 0x25359602, 0xfc70, 0x4d13,	\
		{ 0xa9, 0xab, 0x80, 0x86, 0xd3, 0x82, 0x7c, 0xd }}


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
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IUNICODEDECODER_IID)

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
   * However, we should keep in mind that we need to be lax in decoding. When
   * a decoding error is returned to the caller, it is the caller's
   * responsibility to advance over the bad byte (unless aSrcLength is -1 in
   * which case the caller should call the decoder with 0 offset again) and
   * reset the decoder before trying to call the decoder again.
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
   *                    conversion will contain the number of bytes read or
   *                    -1 on error to indicate that the caller should re-push
   *                    the same buffer after resetting the decoder
   * @param aDest       [OUT] the destination data buffer
   * @param aDestLength [IN/OUT] the length of the destination data buffer;
   *                    after conversion will contain the number of Unicode
   *                    characters written
   * @return            NS_PARTIAL_MORE_INPUT if only a partial conversion was
   *                    done; more input is needed to continue
   *                    NS_PARTIAL_MORE_OUTPUT if only  a partial conversion
   *                    was done; more output space is needed to continue
   *                    NS_ERROR_ILLEGAL_INPUT if an illegal input sequence
   *                    was encountered and the behavior was set to "signal";
   *                    the caller must skip over one byte, reset the decoder
   *                    and retry.
   */
  NS_IMETHOD Convert(const char * aSrc, int32_t * aSrcLength, 
      PRUnichar * aDest, int32_t * aDestLength) = 0;

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
  NS_IMETHOD GetMaxLength(const char * aSrc, int32_t aSrcLength, 
      int32_t * aDestLength) = 0;

  /**
   * Resets the charset converter so it may be recycled for a completely 
   * different and urelated buffer of data.
   */
  NS_IMETHOD Reset() = 0;

  /**
   * Specify what to do when a character cannot be mapped into unicode
   *
   * @param aBehavior [IN] the desired behavior
   * @see kOnError_Recover
   * @see kOnError_Signal
   */
  virtual void SetInputErrorBehavior(int32_t aBehavior) = 0;

  /**
   * return the UNICODE character for unmapped character
   */
  virtual PRUnichar GetCharacterForUnMapped() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIUnicodeDecoder, NS_IUNICODEDECODER_IID)

#endif /* nsIUnicodeDecoder_h___ */
