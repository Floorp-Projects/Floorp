/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright © 2015, Deutsche Telekom, Inc. */

/* Object Directory File (ODF) is an elementary file which contain
   pointers to other EFs. It is specified in PKCS#15 section 6.7. */
this.ODF_DF = [0x50, 0x31];

/* ISO 7816-4: secure messaging */
this.CLA_SM = 0x00;

/* ISO 7816-4, 5.4.1 table 11 */
this.INS_SF = 0xA4; // select file
this.INS_GR = 0xC0; // get response
this.INS_RB = 0xB0; // read binary

/* ISO 7816-4: select file, see 6.11.3, table 58 & 59 */
this.P1_SF_DF = 0x00; // select DF
this.P2_SF_FCP = 0x04; // return FCP

/* ISO 7816-4: read binary, 6.1.3. P1 and P2 describe offset of the first byte
   to be read. We always read the whole files at the moment. */
this.P1_RB = 0x00;
this.P2_RB = 0x00;

/* ISO 7816-4: get response, 7.1.3 table 74,  P1-P2 '0000' (other values RFU) */
this.P1_GR = 0x00;
this.P2_GR = 0x00;

/* ISO 7816-4: 5.1.5 File Control Information, Table 1. For FCP and FMD. */
this.TAG_PROPRIETARY = 0x00;
this.TAG_NON_TLV = 0x53;
this.TAG_BER_TLV = 0x73;

/* ASN.1 tags */
this.TAG_SEQUENCE = 0x30;
this.TAG_OCTETSTRING = 0x04;
this.TAG_OID = 0x06; // Object Identifier

/* ISO 7816-4: 5.1.5 File Control Information, Templates. */
this.TAG_FCP = 0x62; // File control parameters template
this.TAG_FMD = 0x64; // File management data template
this.TAG_FCI = 0x6F; // File control information template

/* EF_DIR tags */
this.TAG_APPLTEMPLATE = 0x61;
this.TAG_APPLIDENTIFIER = 0x4F;
this.TAG_APPLLABEL = 0x50;
this.TAG_APPLPATH = 0x51;

this.TAG_GPD_ALL = 0x82; // EF-ACRules - GPD spec. "all applets"

/* Generic TLVs that are parsed */
this.TAG_GPD_AID = 0xA0; // AID in the EF-ACRules - GPD spec, "one applet"
this.TAG_EXTERNALDO = 0xA1; // External data objects - PKCS#15
this.TAG_INDIRECT = 0xA5; // Indirect value.
this.TAG_EF_ODF = 0xA7; // Elemenetary File Object Directory File

// Allow this file to be imported via Components.utils.import().
this.EXPORTED_SYMBOLS = Object.keys(this);
