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

#ifndef nsISO88591ToUnicode_h___
#define nsISO88591ToUnicode_h___

// Class ID for our ISO88591ToUnicode charset converter
// {A3254CB0-8E20-11d2-8A98-00600811A836}
#define NS_ISO88591TOUNICODE_CID \
  { 0xa3254cb0, 0x8e20, 0x11d2, {0x8a, 0x98, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

#define NS_ISO88591TOUNICODE_CONTRACTID "@mozilla.org/intl/unicode/decoder;1?charset=ISO-8859-1"

//----------------------------------------------------------------------
// Class nsISO88591ToUnicode [declaration]

/**
 * A character set converter from ISO88591 to Unicode.
 *
 * @created         23/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsISO88591ToUnicode : public nsOneByteDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsISO88591ToUnicode();

};

#endif /* nsISO88591ToUnicode_h___ */
