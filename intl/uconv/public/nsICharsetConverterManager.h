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

#ifndef nsICharsetConverterManager_h___
#define nsICharsetConverterManager_h___

#include "nsString.h"
#include "nsError.h"
#include "nsISupports.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"

#define NS_ICHARSETCONVERTERMANAGER_IID \
  {0x3c1c0161, 0x9bd0, 0x11d3, { 0x9d, 0x9, 0x0, 0x50, 0x4, 0x0, 0x7, 0xb2}}

// XXX change to NS_CHARSETCONVERTERMANAGER_CID
#define NS_ICHARSETCONVERTERMANAGER_CID \
  {0x3c1c0163, 0x9bd0, 0x11d3, { 0x9d, 0x9, 0x0, 0x50, 0x4, 0x0, 0x7, 0xb2}}

// XXX change to NS_CHARSETCONVERTERMANAGER_PID
#define NS_CHARSETCONVERTERMANAGER_PROGID "charset-converter-manager"


#define NS_REGISTRY_UCONV_BASE "software/netscape/intl/uconv/"

#define NS_ERROR_UCONV_NOCONV \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_UCONV, 0x01)

#define SET_FOR_BROWSER(_val) (_val |= 0x00000001)
#define SET_NOT_FOR_BROWSER(_val) (_val |= (!0x00000001))
#define GET_FOR_BROWSER(_val) ((_val &= 0x00000001) != 0)

#define SET_FOR_EDITOR(_val) (_val |= 0x00000001)
#define SET_NOT_FOR_EDITOR(_val) (_val |= (!0x00000001))
#define GET_FOR_EDITOR(_val) ((_val &= 0x00000001) != 0)

#define SET_FOR_MAILNEWS(_val) (_val |= 0x00000002)
#define SET_NOT_FOR_MAILNEWS(_val) (_val |= (!0x00000002))
#define GET_FOR_MAILNEWS(_val) ((_val &= 0x00000002) != 0)

#define SET_FOR_MAILNEWSEDITOR(_val) (_val |= 0x00000002)
#define SET_NOT_FOR_MAILNEWSEDITOR(_val) (_val |= (!0x00000002))
#define GET_FOR_MAILNEWSEDITOR(_val) ((_val &= 0x00000002) != 0)

/**
 * Interface for a Manager of Charset Converters.
 * 
 * This Manager's data is a caching of Registry available stuff. But the access 
 * methods are also doing all the work to get it and provide it.
 * 
 * Note: The term "Charset" used in the classes, interfaces and file names 
 * should be read as "Coded Character Set". I am saying "charset" only for 
 * length considerations: it is a much shorter word. This convention is for 
 * source-code only, in the attached documents I will be either using the 
 * full expression or I'll specify a different convention.
 *
 * A DECODER converts from a random encoding into Unicode.
 * An ENCODER converts from Unicode into a random encoding.
 * All our data structures and APIs are divided like that.
 *
 * @created         15/Nov/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsICharsetConverterManager : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICHARSETCONVERTERMANAGER_IID)

  NS_IMETHOD GetUnicodeEncoder(const nsString * aDest, 
      nsIUnicodeEncoder ** aResult) = 0;
  NS_IMETHOD GetUnicodeDecoder(const nsString * aSrc, 
      nsIUnicodeDecoder ** aResult) = 0;
  NS_IMETHOD GetDecoderList(nsString *** aResult, PRInt32 * aCount) = 0;
  NS_IMETHOD GetEncoderList(nsString *** aResult, PRInt32 * aCount) = 0;
  NS_IMETHOD GetDecoderFlags(nsString * aName, PRInt32 * aFlags) = 0;
  NS_IMETHOD GetEncoderFlags(nsString * aName, PRInt32 * aFlags) = 0;
};

#endif /* nsICharsetConverterManager_h___ */
