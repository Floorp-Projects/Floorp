/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIUnicodeEncoder_h___
#define nsIUnicodeEncoder_h___

#include "nscore.h"
#include "nsError.h"
#include "nsISupports.h"

// Interface ID for our Unicode Encoder interface
// {2B2CA3D0-A4C9-11d2-8AA1-00600811A836}
#define NS_IUNICODEENCODER_IID \
	{ 0x2b2ca3d0, 0xa4c9, 0x11d2, \
		{ 0x8a, 0xa1, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36 }}  

// Interface ID for our Unicode Character Encoder interface
// {299BCCD0-C6DF-11d2-8AA8-00600811A836}
#define NS_IUNICHARENCODER_IID	\
	{ 0x299bccd0, 0xc6df, 0x11d2, \
		{0x8a, 0xa8, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36 }}


#define NS_UNICODEENCODER_CONTRACTID_BASE "@mozilla.org/intl/unicode/encoder;1?charset="

/**
 * Interface which converts a single character from Unicode into a given 
 * charset.
 *
 * @created         17/Feb/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsIUnicharEncoder : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IUNICHARENCODER_IID)

  /**
   * Converts a character from Unicode to a Charset.
   */
  NS_IMETHOD Convert(char16_t aChar, char * aDest, int32_t * aDestLength) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIUnicharEncoder, NS_IUNICHARENCODER_IID)

//
// Malloc an Encoder (unicode -> charset) buffer if the
// result won't fit in the static buffer
//
//    p = the buffer pointer   (char*)
//    e = encoder              (nsIUnicodeEncoder*)
//    s = string               (char16_t*)
//    l = string length        (int32_t)
//   sb = static buffer        (char[])
//  sbl = static buffer length (uint32_t)
//   al = actual buffer length (int32_t)
//
#define ENCODER_BUFFER_ALLOC_IF_NEEDED(p,e,s,l,sb,sbl,al) \
  PR_BEGIN_MACRO                                          \
    if (e                                                 \
        && NS_SUCCEEDED((e)->GetMaxLength((s), (l), &(al)))\
        && ((al) > (int32_t)(sbl))                        \
        && (nullptr!=((p)=(char*)nsMemory::Alloc((al)+1))) \
        ) {                                               \
    }                                                     \
    else {                                                \
      (p) = (char*)(sb);                                  \
      (al) = (sbl);                                       \
    }                                                     \
  PR_END_MACRO 

//
// Free the Encoder buffer if it was allocated
//
#define ENCODER_BUFFER_FREE_IF_NEEDED(p,sb) \
  PR_BEGIN_MACRO                            \
    if ((p) != (char*)(sb))                 \
      nsMemory::Free(p);                    \
  PR_END_MACRO 

/**
 * Interface for a Converter from Unicode into a Charset.
 *
 * @created         23/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsIUnicodeEncoder : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IUNICODEENCODER_IID)

  enum {
    kOnError_Signal,        // on an error, stop and signal
    kOnError_CallBack,      // on an error, call the error handler
    kOnError_Replace       // on an error, replace with a different character
  };

  /**
   * Converts the data from Unicode to a Charset.
   *
   * About the byte ordering:
   * - The input stream is Unicode, having the byte order which is internal
   * for the machine on which the converter is running on.
   * - For output, if the converter cares (that depends of the charset, for 
   * example a singlebyte will ignore the byte ordering) it should assume 
   * network order. If necessary and requested, we can add a method 
   * SetOutputByteOrder() so that the reverse order can be used, too. That 
   * method would have as default the assumed network order.
   *
   * For the last converted char, even if there is not enough output 
   * space, a partial output must be done until all available space will be 
   * used. The rest of the output should be buffered until more space becomes
   * available. But this is not also true about the error handling method!!!
   * So be very, very careful...
   *
   * @param aSrc        [IN] the source data buffer
   * @param aSrcLength  [IN/OUT] the length of source data buffer; after
   *                    conversion will contain the number of Unicode 
   *                    characters read
   * @param aDest       [OUT] the destination data buffer
   * @param aDestLength [IN/OUT] the length of the destination data buffer;
   *                    after conversion will contain the number of bytes
   *                    written
   * @return            NS_OK_UENC_MOREOUTPUT if only  a partial conversion
   *                    was done; more output space is needed to continue
   *                    NS_OK_UENC_MOREINPUT if only a partial conversion
   *                    was done; more input is needed to continue. This can
   *                    occur when the last UTF-16 code point in the input is
   *                    the first of a surrogate pair.
   *                    NS_ERROR_UENC_NOMAPPING if character without mapping
   *                    was encountered and the behavior was set to "signal".
   */
  NS_IMETHOD Convert(const char16_t * aSrc, int32_t * aSrcLength, 
      char * aDest, int32_t * aDestLength) = 0;

  /**
   * Finishes the conversion. The converter has the possibility to write some 
   * extra data and flush its final state.
   *
   * @param aDest       [OUT] the destination data buffer
   * @param aDestLength [IN/OUT] the length of destination data buffer; after
   *                    conversion it will contain the number of bytes written
   * @return            NS_OK_UENC_MOREOUTPUT if only  a partial conversion
   *                    was done; more output space is needed to continue
   */
  NS_IMETHOD Finish(char * aDest, int32_t * aDestLength) = 0;

  /**
   * Returns a quick estimation of the size of the buffer needed to hold the
   * converted data. Remember: this estimation is >= with the actual size of 
   * the buffer needed. It will be computed for the "worst case"
   *
   * @param aSrc        [IN] the source data buffer
   * @param aSrcLength  [IN] the length of source data buffer
   * @param aDestLength [OUT] the needed size of the destination buffer
   * @return            NS_OK_UENC_EXACTLENGTH if an exact length was computed
   *                    NS_OK if all we have is an approximation
   */
  NS_IMETHOD GetMaxLength(const char16_t * aSrc, int32_t aSrcLength, 
      int32_t * aDestLength) = 0;

  /**
   * Resets the charset converter so it may be recycled for a completely 
   * different and urelated buffer of data.
   */
  NS_IMETHOD Reset() = 0;

  /**
   * Specify what to do when a character cannot be mapped into the dest charset
   *
   * @param aOrder      [IN] the behavior; taken from the enum
   */
  NS_IMETHOD SetOutputErrorBehavior(int32_t aBehavior, 
      nsIUnicharEncoder * aEncoder, char16_t aChar) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIUnicodeEncoder, NS_IUNICODEENCODER_IID)

#endif /* nsIUnicodeEncoder_h___ */
