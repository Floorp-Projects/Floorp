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

#ifndef nsUCvLatinCID_h___
#define nsUCvLatinCID_h___

#include "nsISupports.h"

// Class ID for our ISO88591ToUnicode charset converter
// {A3254CB0-8E20-11d2-8A98-00600811A836}
NS_DECLARE_ID(kISO88591ToUnicodeCID,
  0xa3254cb0, 0x8e20, 0x11d2, 0x8a, 0x98, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our ISO88592ToUnicode charset converter
// {7C657D11-EC5E-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kISO88592ToUnicodeCID,
  0x7c657d11, 0xec5e, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our ISO88597ToUnicode charset converter
// {AF7A9951-AA48-11d2-B3AE-00805F8A6670}
NS_DECLARE_ID(kISO88597ToUnicodeCID, 
  0xaf7a9951, 0xaa48, 0x11d2, 0xb3, 0xae, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our ISO88599ToUnicode charset converter
// {7C657D13-EC5E-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kISO88599ToUnicodeCID, 
  0x7c657d13, 0xec5e, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our CP1250ToUnicode charset converter
// {7C657D14-EC5E-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kCP1250ToUnicodeCID, 
  0x7c657d14, 0xec5e, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our CP1252ToUnicode charset converter
// {7C657D15-EC5E-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kCP1252ToUnicodeCID, 
  0x7c657d15, 0xec5e, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our CP1253ToUnicode charset converter
// {AF7A9952-AA48-11d2-B3AE-00805F8A6670}
NS_DECLARE_ID(kCP1253ToUnicodeCID, 
  0xaf7a9952, 0xaa48, 0x11d2, 0xb3, 0xae, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our CP1254ToUnicode charset converter
// {7C657D17-EC5E-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kCP1254ToUnicodeCID, 
  0x7c657d17, 0xec5e, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our MacRomanToUnicode charset converter
// {7B8556A1-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kMacRomanToUnicodeCID, 
  0x7b8556a1, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our MacRomanToUnicode charset converter
// {7B8556A2-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kMacCEToUnicodeCID, 
  0x7b8556a2, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our MacGreekToUnicode charset converter
// {7B8556A3-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kMacGreekToUnicodeCID, 
  0x7b8556a3, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our MacTurkishToUnicode charset converter
// {7B8556A4-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kMacTurkishToUnicodeCID, 
  0x7b8556a4, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UTF8ToUnicode charset converter
// {5534DDC0-DD96-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUTF8ToUnicodeCID, 
  0x5534ddc0, 0xdd96, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToISO88591 charset converter
// {920307B0-C6E8-11d2-8AA8-00600811A836}
NS_DECLARE_ID(kUnicodeToISO88591CID, 
  0x920307b0, 0xc6e8, 0x11d2, 0x8a, 0xa8, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToISO88592 charset converter
// {7B8556A6-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToISO88592CID, 
  0x7b8556a6, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToISO88597 charset converter
// {7B8556A8-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToISO88597CID, 
  0x7b8556a8, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToISO88599 charset converter
// {7B8556A9-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToISO88599CID, 
  0x7b8556a9, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToCP1250 charset converter
// {7B8556AA-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToCP1250CID, 
  0x7b8556aa, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToCP1252 charset converter
// {7B8556AC-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToCP1252CID, 
  0x7b8556ac, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToCP1253 charset converter
// {7B8556AD-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToCP1253CID, 
  0x7b8556ad, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToCP1254 charset converter
// {7B8556AE-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToCP1254CID, 
  0x7b8556ae, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToMacRoman charset converter
// {7B8556AF-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToMacRomanCID, 
  0x7b8556af, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToMacCE charset converter
// {7B8556B0-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToMacCECID, 
  0x7b8556b0, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToMacGreek charset converter
// {7B8556B1-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToMacGreekCID, 
  0x7b8556b1, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToMacTurkish charset converter
// {7B8556B2-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToMacTurkishCID, 
  0x7b8556b2, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToUTF8 charset converter
// {7C657D18-EC5E-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToUTF8CID, 
  0x7c657d18, 0xec5e, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

#endif /* nsUCvLatinCID_h___ */
