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

// Class ID for our ISO88593ToUnicode charset converter
// {660D8CA0-F763-11d2-8AAD-00600811A836}
NS_DECLARE_ID(kISO88593ToUnicodeCID,
  0x660d8ca0, 0xf763, 0x11d2, 0x8a, 0xad, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our ISO88594ToUnicode charset converter
// {660D8CA1-F763-11d2-8AAD-00600811A836}
NS_DECLARE_ID(kISO88594ToUnicodeCID,
  0x660d8ca1, 0xf763, 0x11d2, 0x8a, 0xad, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our ISO88595ToUnicode charset converter
// {660D8CA2-F763-11d2-8AAD-00600811A836}
NS_DECLARE_ID(kISO88595ToUnicodeCID,
  0x660d8ca2, 0xf763, 0x11d2, 0x8a, 0xad, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our ISO88596ToUnicode charset converter
// {660D8CA3-F763-11d2-8AAD-00600811A836}
NS_DECLARE_ID(kISO88596ToUnicodeCID,
  0x660d8ca3, 0xf763, 0x11d2, 0x8a, 0xad, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our ISO88597ToUnicode charset converter
// {AF7A9951-AA48-11d2-B3AE-00805F8A6670}
NS_DECLARE_ID(kISO88597ToUnicodeCID, 
  0xaf7a9951, 0xaa48, 0x11d2, 0xb3, 0xae, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our ISO88598ToUnicode charset converter
// {660D8CA4-F763-11d2-8AAD-00600811A836}
NS_DECLARE_ID(kISO88598ToUnicodeCID, 
  0x660d8ca4, 0xf763, 0x11d2, 0x8a, 0xad, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our ISO88599ToUnicode charset converter
// {7C657D13-EC5E-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kISO88599ToUnicodeCID, 
  0x7c657d13, 0xec5e, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our ISO885914ToUnicode charset converter
// {6394EEA1-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kISO885914ToUnicodeCID, 
  0x6394eea1, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our ISO885914ToUnicode charset converter
// {6394EEA2-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kISO885915ToUnicodeCID, 
0x6394eea2, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);


// Class ID for our CP1250ToUnicode charset converter
// {7C657D14-EC5E-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kCP1250ToUnicodeCID, 
  0x7c657d14, 0xec5e, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our CP1251ToUnicode charset converter
// {A578E0A1-F76B-11d2-8AAD-00600811A836}
NS_DECLARE_ID(kCP1251ToUnicodeCID, 
  0xa578e0a1, 0xf76b, 0x11d2, 0x8a, 0xad, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

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

// Class ID for our CP1257ToUnicode charset converter
// {A578E0A2-F76B-11d2-8AAD-00600811A836}
NS_DECLARE_ID(kCP1257ToUnicodeCID, 
  0xa578e0a2, 0xf76b, 0x11d2, 0x8a, 0xad, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our CP1258ToUnicode charset converter
// {6394EEA3-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kCP1258ToUnicodeCID, 
  0x6394eea3, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our CP874ToUnicode charset converter
// {6394EEA4-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kCP874ToUnicodeCID, 
  0x6394eea4, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our KOI8RToUnicode charset converter
// {6394EEA5-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kKOI8RToUnicodeCID, 
  0x6394eea5, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our KOI8UToUnicode charset converter
// {6394EEA6-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kKOI8UToUnicodeCID, 
  0x6394eea6, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our MacRomanToUnicode charset converter
// {7B8556A1-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kMacRomanToUnicodeCID, 
  0x7b8556a1, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our MacCEToUnicode charset converter
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

// Class ID for our MacCroatianToUnicode charset converter
// {6394EEA7-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kMacCroatianToUnicodeCID, 
  0x6394eea7, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our MacRomanianToUnicode charset converter
// {6394EEA8-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kMacRomanianToUnicodeCID, 
  0x6394eea8, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our MacCyrillicToUnicode charset converter
// {6394EEA9-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kMacCyrillicToUnicodeCID, 
  0x6394eea9, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our MacUkrainianToUnicode charset converter
// {6394EEAA-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kMacUkrainianToUnicodeCID, 
  0x6394eeaa, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our MacIcelandicToUnicode charset converter
// {6394EEAB-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kMacIcelandicToUnicodeCID, 
  0x6394eeab, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our ARMSCII8ToUnicode charset converter
// {6394EEAC-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kARMSCII8ToUnicodeCID, 
  0x6394eeac, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our TCVN5712ToUnicode charset converter
// {6394EEAD-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kTCVN5712ToUnicodeCID, 
  0x6394eead, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our VISCIIToUnicode charset converter
// {6394EEAE-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kVISCIIToUnicodeCID, 
  0x6394eeae, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our VPSToUnicode charset converter
// {6394EEB0-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kVPSToUnicodeCID, 
  0x6394eeb0, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our UTF8ToUnicode charset converter
// {5534DDC0-DD96-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUTF8ToUnicodeCID, 
  0x5534ddc0, 0xdd96, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);


// {920307B0-C6E8-11d2-8AA8-00600811A836}
NS_DECLARE_ID(kUnicodeToISO88591CID, 
  0x920307b0, 0xc6e8, 0x11d2, 0x8a, 0xa8, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToISO88592 charset converter
// {7B8556A6-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToISO88592CID, 
  0x7b8556a6, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToISO88593 charset converter
// {660D8CA5-F763-11d2-8AAD-00600811A836}
NS_DECLARE_ID(kUnicodeToISO88593CID, 
  0x660d8ca5, 0xf763, 0x11d2, 0x8a, 0xad, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToISO88594 charset converter
// {660D8CA6-F763-11d2-8AAD-00600811A836}
NS_DECLARE_ID(kUnicodeToISO88594CID, 
  0x660d8ca6, 0xf763, 0x11d2, 0x8a, 0xad, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToISO88595 charset converter
// {660D8CA7-F763-11d2-8AAD-00600811A836}
NS_DECLARE_ID(kUnicodeToISO88595CID, 
  0x660d8ca7, 0xf763, 0x11d2, 0x8a, 0xad, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToISO88596 charset converter
// {660D8CA8-F763-11d2-8AAD-00600811A836}
NS_DECLARE_ID(kUnicodeToISO88596CID, 
  0x660d8ca8, 0xf763, 0x11d2, 0x8a, 0xad, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToISO88597 charset converter
// {7B8556A8-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToISO88597CID, 
  0x7b8556a8, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToISO88598 charset converter
// {660D8CA9-F763-11d2-8AAD-00600811A836}
NS_DECLARE_ID(kUnicodeToISO88598CID, 
  0x660d8ca9, 0xf763, 0x11d2, 0x8a, 0xad, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToISO88599 charset converter
// {7B8556A9-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToISO88599CID, 
  0x7b8556a9, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToISO885914 charset converter
// {6394EEB1-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kUnicodeToISO885914CID, 
  0x6394eeb1, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our UnicodeToISO885915 charset converter
// {6394EEB2-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kUnicodeToISO885915CID, 
  0x6394eeb2, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our UnicodeToCP1250 charset converter
// {7B8556AA-EC79-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToCP1250CID, 
  0x7b8556aa, 0xec79, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToCP1251 charset converter
// {A578E0A3-F76B-11d2-8AAD-00600811A836}
NS_DECLARE_ID(kUnicodeToCP1251CID, 
  0xa578e0a3, 0xf76b, 0x11d2, 0x8a, 0xad, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

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

// Class ID for our UnicodeToCP1257 charset converter
// {A578E0A4-F76B-11d2-8AAD-00600811A836}
NS_DECLARE_ID(kUnicodeToCP1257CID, 
  0xa578e0a4, 0xf76b, 0x11d2, 0x8a, 0xad, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToCP1258 charset converter
// {6394EEB3-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kUnicodeToCP1258CID, 
  0x6394eeb3, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our UnicodeToCP874 charset converter
// {6394EEB4-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kUnicodeToCP874CID, 
  0x6394eeb4, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our UnicodeToKOI8R charset converter
// {6394EEB5-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kUnicodeToKOI8RCID, 
  0x6394eeb5, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our UnicodeToKOI8U charset converter
// {6394EEB6-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kUnicodeToKOI8UCID, 
  0x6394eeb6, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

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

// Class ID for our UnicodeToMacCroatian charset converter
// {6394EEB7-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kUnicodeToMacCroatianCID, 
  0x6394eeb7, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our UnicodeToMacRomanian charset converter
// {6394EEB8-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kUnicodeToMacRomanianCID, 
  0x6394eeb8, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our UnicodeToMacCyrillic charset converter
// {6394EEB9-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kUnicodeToMacCyrillicCID, 
  0x6394eeb9, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our UnicodeToMacUkrainian charset converter
// {6394EEBA-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kUnicodeToMacUkrainianCID,
  0x6394eeba, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our UnicodeToMacIcelandic charset converter
// {6394EEBB-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kUnicodeToMacIcelandicCID, 
  0x6394eebb, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our UnicodeToARMSCII8 charset converter
// {6394EEBC-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kUnicodeToARMSCII8CID, 
  0x6394eebc, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our UnicodeToTCVN5712 charset converter
// {6394EEBD-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kUnicodeToTCVN5712CID, 
  0x6394eebd, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our UnicodeToVISCII charset converter
// {6394EEBF-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kUnicodeToVISCIICID, 
  0x6394eebf, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our UnicodeToVPS charset converter
// {6394EEC0-FC3D-11d2-B3B8-00805F8A6670}
NS_DECLARE_ID(kUnicodeToVPSCID, 
  0x6394eec0, 0xfc3d, 0x11d2, 0xb3, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);

// Class ID for our UnicodeToUTF8 charset converter
// {7C657D18-EC5E-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToUTF8CID, 
  0x7c657d18, 0xec5e, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);


#endif /* nsUCvLatinCID_h___ */
