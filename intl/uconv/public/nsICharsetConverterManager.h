/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsICharsetConverterManager_h___
#define nsICharsetConverterManager_h___

#include "nsString.h"
#include "nsError.h"
#include "nsISupports.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"

// Interface ID for our ConverterManager interface
// {1E3F79F0-6B6B-11d2-8A86-00600811A836}
// NS_DECLARE_ID(kICharsetConverterManagerIID,
//  0x1e3f79f0, 0x6b6b, 0x11d2, 0x8a, 0x86, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

#define NS_ICHARSETCONVERTERMANAGER_IID											 \
{ /* 1E3F79F0-6B6B-11d2-8A86-00600811A836 */         \
    0x1e3f79f0,                                      \
    0x6b6b,                                          \
    0x11d2,                                          \
    {0x8a, 0x86, 0x00, 0x60, 0x08, 0x11, 0xa8, 0x36} \
}

// Class ID for our ConverterManager implementation
// {1E3F79F1-6B6B-11d2-8A86-00600811A836}
// NS_DECLARE_ID(kCharsetConverterManagerCID, 
//  0x1e3f79f1, 0x6b6b, 0x11d2, 0x8a, 0x86, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

#define NS_ICHARSETCONVERTERMANAGER_CID											 \
{ /* 1E3F79F1-6B6B-11d2-8A86-00600811A836 */         \
    0x1e3f79f1,                                      \
    0x6b6b,                                          \
    0x11d2,                                          \
    {0x8a, 0x86, 0x00, 0x60, 0x08, 0x11, 0xa8, 0x36} \
}

#define NS_CHARSETCONVERTERMANAGER_PROGID "component://netscape/intl/charsetconvertermanager"

#define NS_ERROR_UCONV_NOCONV \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_UCONV,1)

/**
 * Interface for a Manager of Charset Converters.
 *
 * Note: The term "Charset" used in the classes, interfaces and file names 
 * should be read as "Coded Character Set". I am saying "charset" only for 
 * length considerations: it is a much shorter word. This convention is for 
 * source-code only, in the attached documents I will be either using the 
 * full expression or I'll specify a different convetion.
 *
 * XXX Move the ICharsetManager methods (the last 2 methods in the interface) 
 * into a separate interface. They are conceptually independent. I left them 
 * here only for easier implementation.
 *
 * XXX Add HasUnicodeEncoder() and HasUnicodeDecoder() methods.
 *
 * @created         17/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsICharsetConverterManager : public nsISupports
{
public:

  static const nsIID& GetIID() { static nsIID iid = NS_ICHARSETCONVERTERMANAGER_IID; return iid; }
  /**
   * Find and instantiate a Converter able to encode from Unicode into the 
   * destination charset.
   *
   * @param aDest   [IN] the known name/alias of the destination charset
   * @param aResult [OUT] the charset converter
   * @return        NS_ERROR_UCONV_NOCONV if no converter was found for
   *                this charset
   */
  NS_IMETHOD GetUnicodeEncoder(const nsString * aDest, 
      nsIUnicodeEncoder ** aResult) = 0;

  /**
   * Find and instantiate a Converter able to decode from the source charset 
   * into Unicode.
   *
   * @param aSrc    [IN] the known name/alias of the source charset
   * @param aResult [OUT] the charset converter
   * @return        NS_ERROR_UCONV_NOCONV if no converter was found for
   *                this charset
   */
  NS_IMETHOD GetUnicodeDecoder(const nsString * aSrc, 
      nsIUnicodeDecoder ** aResult) = 0;

  /**
   * Returns a list of charsets for which we have converters from Unicode.
   *
   * @param aResult     [OUT] an array of pointers to Strings (charset names)
   * @param aCount      [OUT] the size (number of elements) of that array
   */
  NS_IMETHOD GetEncodableCharsets(nsString *** aResult, PRInt32 * aCount) = 0;

  /**
   * Returns a list of charsets for which we have converters into Unicode.
   *
   * @param aResult     [OUT] an array of pointers to Strings (charset names)
   * @param aCount      [OUT] the size (number of elements) of that array
   */
  NS_IMETHOD GetDecodableCharsets(nsString *** aResult, PRInt32 * aCount) = 0;

  /**
   * Resolves the canonical name of a charset. If the given name is unknown 
   * to the resolver, a new identical string will be returned! This way, 
   * new & unknown charsets are not rejected and they are treated as any other
   * charset except they can't have aliases.
   *
   * @param aCharset    [IN] the known name/alias of the character set
   * @param aResult     [OUT] the canonical name of the character set
   */
  NS_IMETHOD GetCharsetName(const nsString * aCharset, 
      nsString ** aResult) = 0;

  /**
   * Returns a list containing all the legal names for a given charset. The
   * list will allways have at least 1 element: the cannonical name for that
   * charset.
   *
   * @param aCharset    [IN] a known name/alias of the character set
   * @param aResult     [OUT] the list; in the first position is the 
   *                    cannonical name, then the other aliases
   */
  NS_IMETHOD GetCharsetNames(const nsString * aCharset, 
      nsString *** aResult, PRInt32 * aCount) = 0;
};

#endif /* nsICharsetConverterManager_h___ */
