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

  mDecSize           = 20;
  mDecArray          = new ConverterInfo [mDecSize];

  mDecArray[0].mCID  = &kISO88591ToUnicodeCID;
  mDecArray[1].mCID  = &kISO88592ToUnicodeCID;
  mDecArray[2].mCID  = &kISO88597ToUnicodeCID;
  mDecArray[3].mCID  = &kISO88599ToUnicodeCID;
  mDecArray[4].mCID  = &kCP1250ToUnicodeCID;
  mDecArray[5].mCID  = &kCP1252ToUnicodeCID;
  mDecArray[6].mCID  = &kCP1253ToUnicodeCID;
  mDecArray[7].mCID  = &kCP1254ToUnicodeCID;
  mDecArray[8].mCID  = &kMacRomanToUnicodeCID;
  mDecArray[9].mCID  = &kMacCEToUnicodeCID;
  mDecArray[10].mCID  = &kMacGreekToUnicodeCID;
  mDecArray[11].mCID  = &kMacTurkishToUnicodeCID;
  mDecArray[12].mCID  = &kUTF8ToUnicodeCID;

  mDecArray[13].mCID  = &kSJIS2UnicodeCID;
  mDecArray[14].mCID  = &kISO2022JPToUnicodeCID;
  mDecArray[15].mCID  = &kEUCJPToUnicodeCID;

  mDecArray[16].mCID  = &kBIG5ToUnicodeCID;
  mDecArray[17].mCID  = &kEUCTWToUnicodeCID;
  mDecArray[18].mCID  = &kGB2312ToUnicodeCID;
  mDecArray[19].mCID  = &kEUCKRToUnicodeCID;
  
  mEncSize           = 19;
  mEncArray          = new ConverterInfo [mEncSize];

  mEncArray[0].mCID  = &kUnicodeToISO88591CID;
  mEncArray[1].mCID  = &kUnicodeToISO88592CID;
  mEncArray[2].mCID  = &kUnicodeToISO88597CID;
  mEncArray[3].mCID  = &kUnicodeToISO88599CID;
  mEncArray[4].mCID  = &kUnicodeToCP1250CID;
  mEncArray[5].mCID  = &kUnicodeToCP1252CID;
  mEncArray[6].mCID  = &kUnicodeToCP1253CID;
  mEncArray[7].mCID  = &kUnicodeToCP1254CID;
  mEncArray[8].mCID  = &kUnicodeToMacRomanCID;
  mEncArray[9].mCID  = &kUnicodeToMacCECID;
  mEncArray[10].mCID  = &kUnicodeToMacGreekCID;
  mEncArray[11].mCID  = &kUnicodeToMacTurkishCID;
  mEncArray[12].mCID  = &kUnicodeToUTF8CID;

  mEncArray[13].mCID  = &kUnicodeToSJISCID;
  mEncArray[14].mCID  = &kUnicodeToEUCJPCID;

  mEncArray[15].mCID  = &kUnicodeToBIG5CID;
  mEncArray[16].mCID  = &kUnicodeToEUCTWCID;
  mEncArray[17].mCID  = &kUnicodeToGB2312CID;
  mEncArray[18].mCID  = &kUnicodeToEUCKRCID;
