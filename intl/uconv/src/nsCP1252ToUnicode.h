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

#ifndef nsCP1252ToUnicode_h___
#define nsCP1252ToUnicode_h___

// Class ID for our CP1252ToUnicode charset converter
// {7C657D15-EC5E-11d2-8AAC-00600811A836}
#define NS_CP1252TOUNICODE_CID \
  { 0x7c657d15, 0xec5e, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

#define NS_CP1252TOUNICODE_CONTRACTID "@mozilla.org/intl/unicode/decoder;1?charset=windows-1252"

//----------------------------------------------------------------------
// Class nsCP1252ToUnicode [declaration]

/**
 * A character set converter from CP1252 to Unicode.
 *
 * @created         20/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsCP1252ToUnicode : public nsOneByteDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsCP1252ToUnicode();

};

#endif /* nsCP1252ToUnicode_h___ */
