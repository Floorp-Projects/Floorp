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

  mDecSize           = 42;
  mDecArray          = new ConverterInfo [mDecSize];

  mDecArray[0].mCID  = &kISO88591ToUnicodeCID;
  mDecArray[1].mCID  = &kISO88592ToUnicodeCID;
  mDecArray[2].mCID  = &kISO88593ToUnicodeCID;
  mDecArray[3].mCID  = &kISO88594ToUnicodeCID;
  mDecArray[4].mCID  = &kISO88595ToUnicodeCID;
  mDecArray[5].mCID  = &kISO88596ToUnicodeCID;
  mDecArray[6].mCID  = &kISO88597ToUnicodeCID;
  mDecArray[7].mCID  = &kISO88598ToUnicodeCID;
  mDecArray[8].mCID  = &kISO88599ToUnicodeCID;
  mDecArray[9].mCID  = &kCP1250ToUnicodeCID;
  mDecArray[10].mCID  = &kCP1251ToUnicodeCID;
  mDecArray[11].mCID  = &kCP1252ToUnicodeCID;
  mDecArray[12].mCID  = &kCP1253ToUnicodeCID;
  mDecArray[13].mCID  = &kCP1254ToUnicodeCID;
  mDecArray[14].mCID  = &kCP1257ToUnicodeCID;
  mDecArray[15].mCID  = &kMacRomanToUnicodeCID;
  mDecArray[16].mCID  = &kMacCEToUnicodeCID;
  mDecArray[17].mCID  = &kMacGreekToUnicodeCID;
  mDecArray[18].mCID  = &kMacTurkishToUnicodeCID;
  mDecArray[19].mCID  = &kUTF8ToUnicodeCID;

  mDecArray[20].mCID  = &kSJIS2UnicodeCID;
  mDecArray[21].mCID  = &kISO2022JPToUnicodeCID;
  mDecArray[22].mCID  = &kEUCJPToUnicodeCID;

  mDecArray[23].mCID  = &kBIG5ToUnicodeCID;
  mDecArray[24].mCID  = &kEUCTWToUnicodeCID;
  mDecArray[25].mCID  = &kGB2312ToUnicodeCID;
  mDecArray[26].mCID  = &kEUCKRToUnicodeCID;

  mDecArray[27].mCID  = &kISO885914ToUnicodeCID;
  mDecArray[28].mCID  = &kISO885915ToUnicodeCID;
  mDecArray[29].mCID  = &kCP1258ToUnicodeCID;
  mDecArray[30].mCID  = &kCP874ToUnicodeCID;
  mDecArray[31].mCID  = &kMacCroatianToUnicodeCID;
  mDecArray[32].mCID  = &kMacRomanianToUnicodeCID;
  mDecArray[33].mCID  = &kMacCyrillicToUnicodeCID;
  mDecArray[34].mCID  = &kMacUkrainianToUnicodeCID;
  mDecArray[35].mCID  = &kMacIcelandicToUnicodeCID;
  mDecArray[36].mCID  = &kARMSCII8ToUnicodeCID;
  mDecArray[37].mCID  = &kTCVN5712ToUnicodeCID;
  mDecArray[38].mCID  = &kVISCIIToUnicodeCID;
  mDecArray[39].mCID  = &kVPSToUnicodeCID;
  mDecArray[40].mCID  = &kKOI8RToUnicodeCID;
  mDecArray[41].mCID  = &kKOI8UToUnicodeCID;
  
  mEncSize           = 42;
  mEncArray          = new ConverterInfo [mEncSize];

  mEncArray[0].mCID  = &kUnicodeToISO88591CID;
  mEncArray[1].mCID  = &kUnicodeToISO88592CID;
  mEncArray[2].mCID  = &kUnicodeToISO88593CID;
  mEncArray[3].mCID  = &kUnicodeToISO88594CID;
  mEncArray[4].mCID  = &kUnicodeToISO88595CID;
  mEncArray[5].mCID  = &kUnicodeToISO88596CID;
  mEncArray[6].mCID  = &kUnicodeToISO88597CID;
  mEncArray[7].mCID  = &kUnicodeToISO88598CID;
  mEncArray[8].mCID  = &kUnicodeToISO88599CID;
  mEncArray[9].mCID  = &kUnicodeToCP1250CID;
  mEncArray[10].mCID  = &kUnicodeToCP1251CID;
  mEncArray[11].mCID  = &kUnicodeToCP1252CID;
  mEncArray[12].mCID  = &kUnicodeToCP1253CID;
  mEncArray[13].mCID  = &kUnicodeToCP1254CID;
  mEncArray[14].mCID  = &kUnicodeToCP1257CID;
  mEncArray[15].mCID  = &kUnicodeToMacRomanCID;
  mEncArray[16].mCID  = &kUnicodeToMacCECID;
  mEncArray[17].mCID  = &kUnicodeToMacGreekCID;
  mEncArray[18].mCID  = &kUnicodeToMacTurkishCID;
  mEncArray[19].mCID  = &kUnicodeToUTF8CID;

  mEncArray[20].mCID  = &kUnicodeToSJISCID;
  mEncArray[21].mCID  = &kUnicodeToISO2022JPCID;
  mEncArray[22].mCID  = &kUnicodeToEUCJPCID;

  mEncArray[23].mCID  = &kUnicodeToBIG5CID;
  mEncArray[24].mCID  = &kUnicodeToEUCTWCID;
  mEncArray[25].mCID  = &kUnicodeToGB2312CID;
  mEncArray[26].mCID  = &kUnicodeToEUCKRCID;

  mEncArray[27].mCID  = &kUnicodeToISO885914CID;
  mEncArray[28].mCID  = &kUnicodeToISO885915CID;
  mEncArray[29].mCID  = &kUnicodeToCP1258CID;
  mEncArray[30].mCID  = &kUnicodeToCP874CID;
  mEncArray[31].mCID  = &kUnicodeToMacCroatianCID;
  mEncArray[32].mCID  = &kUnicodeToMacRomanianCID;
  mEncArray[33].mCID  = &kUnicodeToMacCyrillicCID;
  mEncArray[34].mCID  = &kUnicodeToMacUkrainianCID;
  mEncArray[35].mCID  = &kUnicodeToMacIcelandicCID;
  mEncArray[36].mCID  = &kUnicodeToARMSCII8CID;
  mEncArray[37].mCID  = &kUnicodeToTCVN5712CID;
  mEncArray[38].mCID  = &kUnicodeToVISCIICID;
  mEncArray[39].mCID  = &kUnicodeToVPSCID;
  mEncArray[40].mCID  = &kUnicodeToKOI8RCID;
  mEncArray[41].mCID  = &kUnicodeToKOI8UCID;
