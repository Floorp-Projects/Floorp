/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#define NS_OK_UENC_EXACTLENGTH      \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_UCONV, 0x21)

#define NS_OK_UENC_MOREOUTPUT       \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_UCONV, 0x22)

#define NS_ERROR_UENC_NOMAPPING     \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_UCONV, 0x23)

#define NS_OK_UENC_MOREINPUT       \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_UCONV, 0x24)


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
  NS_IMETHOD Convert(PRUnichar aChar, char * aDest, PRInt32 * aDestLength) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIUnicharEncoder, NS_IUNICHARENCODER_IID)

//
// Malloc an Encoder (unicode -> charset) buffer if the
// result won't fit in the static buffer
//
//    p = the buffer pointer   (char*)
//    e = encoder              (nsIUnicodeEncoder*)
//    s = string               (PRUnichar*)
//    l = string length        (PRInt32)
//   sb = static buffer        (char[])
//  sbl = static buffer length (PRUint32)
//   al = actual buffer length (PRInt32)
//
#define ENCODER_BUFFER_ALLOC_IF_NEEDED(p,e,s,l,sb,sbl,al) \
  PR_BEGIN_MACRO                                          \
    if (e                                                 \
        && NS_SUCCEEDED((e)->GetMaxLength((s), (l), &(al)))\
        && ((al) > (PRInt32)(sbl))                        \
        && (nsnull!=((p)=(char*)nsMemory::Alloc((al)+1))) \
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
   * space, a partial ouput must be done until all available space will be 
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
  NS_IMETHOD Convert(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength) = 0;

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
  NS_IMETHOD Finish(char * aDest, PRInt32 * aDestLength) = 0;

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
  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength) = 0;

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
  NS_IMETHOD SetOutputErrorBehavior(PRInt32 aBehavior, 
      nsIUnicharEncoder * aEncoder, PRUnichar aChar) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIUnicodeEncoder, NS_IUNICODEENCODER_IID)

#endif /* nsIUnicodeEncoder_h___ */
