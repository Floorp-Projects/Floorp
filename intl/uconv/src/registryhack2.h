/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

  mDecSize           = 61;
  mDecArray          = new ConverterInfo [mDecSize];

  PRInt32 i =0;







  mDecArray[i++].mCID  = &kAsciiToUnicodeCID;
  mDecArray[i++].mCID  = &kUEscapeToUnicodeCID;
  mDecArray[i++].mCID  = &kISO88591ToUnicodeCID;
  mDecArray[i++].mCID  = &kISO88592ToUnicodeCID;
  mDecArray[i++].mCID  = &kISO88593ToUnicodeCID;
  mDecArray[i++].mCID  = &kISO88594ToUnicodeCID;
  mDecArray[i++].mCID  = &kISO88595ToUnicodeCID;
  mDecArray[i++].mCID  = &kISO88596ToUnicodeCID;
  mDecArray[i++].mCID  = &kISO88597ToUnicodeCID;
  mDecArray[i++].mCID  = &kISO88598ToUnicodeCID;
  mDecArray[i++].mCID  = &kISO88599ToUnicodeCID;
  mDecArray[i++].mCID  = &kISO885910ToUnicodeCID;
  mDecArray[i++].mCID  = &kISO885913ToUnicodeCID;
  mDecArray[i++].mCID  = &kISO885914ToUnicodeCID;
  mDecArray[i++].mCID  = &kISO885915ToUnicodeCID;
  mDecArray[i++].mCID  = &kISOIR111ToUnicodeCID;
  mDecArray[i++].mCID  = &kCP1250ToUnicodeCID;
  mDecArray[i++].mCID  = &kCP1251ToUnicodeCID;
  mDecArray[i++].mCID  = &kCP1252ToUnicodeCID;
  mDecArray[i++].mCID  = &kCP1253ToUnicodeCID;
  mDecArray[i++].mCID  = &kCP1254ToUnicodeCID;
  mDecArray[i++].mCID  = &kCP1255ToUnicodeCID;
  mDecArray[i++].mCID  = &kCP1256ToUnicodeCID;
  mDecArray[i++].mCID  = &kCP1257ToUnicodeCID;
  mDecArray[i++].mCID  = &kCP1258ToUnicodeCID;
  mDecArray[i++].mCID  = &kCP874ToUnicodeCID;
  mDecArray[i++].mCID  = &kCP866ToUnicodeCID;
  mDecArray[i++].mCID  = &kMacRomanToUnicodeCID;
  mDecArray[i++].mCID  = &kMacCEToUnicodeCID;
  mDecArray[i++].mCID  = &kMacGreekToUnicodeCID;
  mDecArray[i++].mCID  = &kMacTurkishToUnicodeCID;
  mDecArray[i++].mCID  = &kUTF8ToUnicodeCID;
  mDecArray[i++].mCID  = &kSJIS2UnicodeCID;
  mDecArray[i++].mCID  = &kISO2022JPToUnicodeCID;
  mDecArray[i++].mCID  = &kEUCJPToUnicodeCID;
  mDecArray[i++].mCID  = &kBIG5ToUnicodeCID;
  mDecArray[i++].mCID  = &kEUCTWToUnicodeCID;
  mDecArray[i++].mCID  = &kGB2312ToUnicodeCID;
  mDecArray[i++].mCID  = &kEUCKRToUnicodeCID;
  mDecArray[i++].mCID  = &kMacCroatianToUnicodeCID;
  mDecArray[i++].mCID  = &kMacRomanianToUnicodeCID;
  mDecArray[i++].mCID  = &kMacCyrillicToUnicodeCID;
  mDecArray[i++].mCID  = &kMacUkrainianToUnicodeCID;
  mDecArray[i++].mCID  = &kMacIcelandicToUnicodeCID;
  mDecArray[i++].mCID  = &kARMSCII8ToUnicodeCID;
  mDecArray[i++].mCID  = &kTCVN5712ToUnicodeCID;
  mDecArray[i++].mCID  = &kVISCIIToUnicodeCID;
  mDecArray[i++].mCID  = &kVPSToUnicodeCID;
  mDecArray[i++].mCID  = &kKOI8RToUnicodeCID;
  mDecArray[i++].mCID  = &kKOI8UToUnicodeCID;
  mDecArray[i++].mCID  = &kMUTF7ToUnicodeCID;
  mDecArray[i++].mCID  = &kUTF7ToUnicodeCID;
  mDecArray[i++].mCID  = &kUTF16BEToUnicodeCID;
  mDecArray[i++].mCID  = &kUTF16LEToUnicodeCID;
  mDecArray[i++].mCID  = &kUTF32BEToUnicodeCID;
  mDecArray[i++].mCID  = &kUTF32LEToUnicodeCID;
  mDecArray[i++].mCID  = &kT61ToUnicodeCID;
  mDecArray[i++].mCID  = &kUserDefinedToUnicodeCID;
  mDecArray[i++].mCID  = &kObsSJISToUnicodeCID;
  mDecArray[i++].mCID  = &kObsEUCJPToUnicodeCID;
  mDecArray[i++].mCID  = &kObsISO2022JPToUnicodeCID;

  mEncSize           = 75;
  mEncArray          = new ConverterInfo [mEncSize];

  i = 0;




  mEncArray[i++].mCID  = &kUnicodeToAsciiCID;
  mEncArray[i++].mCID  = &kUnicodeToUEscapeCID;
  mEncArray[i++].mCID  = &kUnicodeToISO88591CID;
  mEncArray[i++].mCID  = &kUnicodeToISO88592CID;
  mEncArray[i++].mCID  = &kUnicodeToISO88593CID;
  mEncArray[i++].mCID  = &kUnicodeToISO88594CID;
  mEncArray[i++].mCID  = &kUnicodeToISO88595CID;
  mEncArray[i++].mCID  = &kUnicodeToISO88596CID;
  mEncArray[i++].mCID  = &kUnicodeToISO88597CID;
  mEncArray[i++].mCID  = &kUnicodeToISO88598CID;
  mEncArray[i++].mCID  = &kUnicodeToISO88599CID;
  mEncArray[i++].mCID  = &kUnicodeToISO885910CID;
  mEncArray[i++].mCID  = &kUnicodeToISO885913CID;
  mEncArray[i++].mCID  = &kUnicodeToISO885914CID;
  mEncArray[i++].mCID  = &kUnicodeToISO885915CID;
  mEncArray[i++].mCID  = &kUnicodeToISOIR111CID;
  mEncArray[i++].mCID  = &kUnicodeToCP1250CID;
  mEncArray[i++].mCID  = &kUnicodeToCP1251CID;
  mEncArray[i++].mCID  = &kUnicodeToCP1252CID;
  mEncArray[i++].mCID  = &kUnicodeToCP1253CID;
  mEncArray[i++].mCID  = &kUnicodeToCP1254CID;
  mEncArray[i++].mCID  = &kUnicodeToCP1255CID;
  mEncArray[i++].mCID  = &kUnicodeToCP1256CID;
  mEncArray[i++].mCID  = &kUnicodeToCP1257CID;
  mEncArray[i++].mCID  = &kUnicodeToCP1258CID;
  mEncArray[i++].mCID  = &kUnicodeToCP874CID;
  mEncArray[i++].mCID  = &kUnicodeToCP866CID;
  mEncArray[i++].mCID  = &kUnicodeToMacRomanCID;
  mEncArray[i++].mCID  = &kUnicodeToMacCECID;
  mEncArray[i++].mCID  = &kUnicodeToMacGreekCID;
  mEncArray[i++].mCID  = &kUnicodeToMacTurkishCID;
  mEncArray[i++].mCID  = &kUnicodeToUTF8CID;
  mEncArray[i++].mCID  = &kUnicodeToSJISCID;
  mEncArray[i++].mCID  = &kUnicodeToISO2022JPCID;
  mEncArray[i++].mCID  = &kUnicodeToEUCJPCID;
  mEncArray[i++].mCID  = &kUnicodeToBIG5CID;
  mEncArray[i++].mCID  = &kUnicodeToEUCTWCID;
  mEncArray[i++].mCID  = &kUnicodeToGB2312CID;
  mEncArray[i++].mCID  = &kUnicodeToEUCKRCID;
  mEncArray[i++].mCID  = &kUnicodeToMacCroatianCID;
  mEncArray[i++].mCID  = &kUnicodeToMacRomanianCID;
  mEncArray[i++].mCID  = &kUnicodeToMacCyrillicCID;
  mEncArray[i++].mCID  = &kUnicodeToMacUkrainianCID;
  mEncArray[i++].mCID  = &kUnicodeToMacIcelandicCID;
  mEncArray[i++].mCID  = &kUnicodeToARMSCII8CID;
  mEncArray[i++].mCID  = &kUnicodeToTCVN5712CID;
  mEncArray[i++].mCID  = &kUnicodeToVISCIICID;
  mEncArray[i++].mCID  = &kUnicodeToVPSCID;
  mEncArray[i++].mCID  = &kUnicodeToKOI8RCID;
  mEncArray[i++].mCID  = &kUnicodeToKOI8UCID;
  mEncArray[i++].mCID  = &kUnicodeToMUTF7CID;
  mEncArray[i++].mCID  = &kUnicodeToUTF7CID;
  mEncArray[i++].mCID  = &kUnicodeToUTF16BECID;
  mEncArray[i++].mCID  = &kUnicodeToUTF16LECID;
  mEncArray[i++].mCID  = &kUnicodeToUTF16CID;
  mEncArray[i++].mCID  = &kUnicodeToUTF32BECID;
  mEncArray[i++].mCID  = &kUnicodeToUTF32LECID;
  mEncArray[i++].mCID  = &kUnicodeToT61CID;
  mEncArray[i++].mCID  = &kUnicodeToUserDefinedCID;
  mEncArray[i++].mCID  = &kUnicodeToJISx0201CID;
  mEncArray[i++].mCID  = &kUnicodeToJISx0208CID;
  mEncArray[i++].mCID  = &kUnicodeToJISx0212CID;
  mEncArray[i++].mCID  = &kUnicodeToKSC5601CID;
  mEncArray[i++].mCID  = &kUnicodeToGB2312GLCID;
  mEncArray[i++].mCID  = &kUnicodeToBIG5NoAsciiCID;
  mEncArray[i++].mCID  = &kUnicodeToCNS11643p1CID;
  mEncArray[i++].mCID  = &kUnicodeToCNS11643p2CID;
  mEncArray[i++].mCID  = &kUnicodeToCNS11643p3CID;
  mEncArray[i++].mCID  = &kUnicodeToCNS11643p4CID;
  mEncArray[i++].mCID  = &kUnicodeToCNS11643p5CID;
  mEncArray[i++].mCID  = &kUnicodeToCNS11643p6CID;
  mEncArray[i++].mCID  = &kUnicodeToCNS11643p7CID;
  mEncArray[i++].mCID  = &kUnicodeToSymbolCID;
  mEncArray[i++].mCID  = &kUnicodeToZapfDingbatsCID;
  mEncArray[i++].mCID  = &kUnicodeToX11JohabCID;

