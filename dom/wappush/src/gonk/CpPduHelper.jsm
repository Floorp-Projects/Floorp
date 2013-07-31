/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let WSP = {};
Cu.import("resource://gre/modules/WspPduHelper.jsm", WSP);
let WBXML = {};
Cu.import("resource://gre/modules/WbxmlPduHelper.jsm", WBXML);

// set to true to see debug messages
let DEBUG = WBXML.DEBUG_ALL | false;

/**
 * Public identifier for CP
 *
 * @see http://technical.openmobilealliance.org/tech/omna/omna-wbxml-public-docid.aspx
 */
const PUBLIC_IDENTIFIER_CP = "-//WAPFORUM//DTD PROV 1.0//EN";

this.PduHelper = {

  /**
    * @param data
    *        A wrapped object containing raw PDU data.
    * @param contentType
    *        Content type of incoming CP message, should be "text/vnd.wap.connectivity-xml"
    *        or "application/vnd.wap.connectivity-wbxml".
    *
    * @return A message object containing attribute content and contentType.
    *         |content| will contain string of decoded CP message if successfully
    *         decoded, or raw data if failed.
    *         |contentType| will be string representing corresponding type of
    *         content.
    */
  parse: function parse_cp(data, contentType) {
    // We only need content and contentType
    let msg = {
      contentType: contentType
    };

    /**
     * Message is compressed by WBXML, decode into string.
     *
     * @see WAP-192-WBXML-20010725-A
     */
    if (contentType === "application/vnd.wap.connectivity-wbxml") {
      let appToken = {
        publicId: PUBLIC_IDENTIFIER_CP,
        tagTokenList: CP_TAG_FIELDS,
        attrTokenList: CP_ATTRIBUTE_FIELDS,
        valueTokenList: CP_VALUE_FIELDS,
        globalTokenOverride: null
      }

      try {
        let parseResult = WBXML.PduHelper.parse(data, appToken, msg);
        msg.content = parseResult.content;
        msg.contentType = "text/vnd.wap.connectivity-xml";
      } catch (e) {
        // Provide raw data if we failed to parse.
        msg.content = data.array;
      }

      return msg;
    }

    /**
     * Message is plain text, transform raw to string.
     */
    try {
      let stringData = WSP.Octet.decodeMultiple(data, data.array.length);
      msg.content = WSP.PduHelper.decodeStringContent(stringData, "UTF-8");
    } catch (e) {
      // Provide raw data if we failed to parse.
      msg.content = data.array;
    }
    return msg;

  }
};

/**
  * Tag tokens
  *
  * @see WAP-183-ProvCont-20010724-A, clause 8.1 for code page 0
  * @see OMA-WAP-TS-ProvCont-V1_1-20090421-C, clause 7.1 for code page 1
  */
const CP_TAG_FIELDS = (function () {
  let names = {};
  function add(name, codepage, number) {
    let entry = {
      name: name,
      number: number,
    };
    if (!names[codepage]) {
      names[codepage] = {};
    }
    names[codepage][number] = entry;
  }

  // Code page 0
  add("wap-provisioningdoc", 0, 0x05);
  add("characteristic",      0, 0x06);
  add("parm",                0, 0x07);
  // Code page 1
  add("characteristic",      1, 0x06);
  add("parm",                1, 0x07);

  return names;
})();

/**
  * Attribute Tokens
  *
  * @see WAP-183-ProvCont-20010724-A, clause 8.2 for code page 0
  * @see OMA-WAP-TS-ProvCont-V1_1-20090421-C, clause 7.2 for code page 1
  */
const CP_ATTRIBUTE_FIELDS = (function () {
  let names = {};
  function add(name, value, codepage, number) {
    let entry = {
      name: name,
      value: value,
      number: number,
    };
    if (!names[codepage]) {
      names[codepage] = {};
    }
    names[codepage][number] = entry;
  }

  // Code page 0
  add("name",     "",                             0,  0x05);
  add("value",    "",                             0,  0x06);
  add("name",     "NAME",                         0,  0x07);
  add("name",     "NAP-ADDRESS",                  0,  0x08);
  add("name",     "NAP-ADDRTYPE",                 0,  0x09);
  add("name",     "CALLTYPE",                     0,  0x0A);
  add("name",     "VALIDUNTIL",                   0,  0x0B);
  add("name",     "AUTHTYPE",                     0,  0x0C);
  add("name",     "AUTHNAME",                     0,  0x0D);
  add("name",     "AUTHSECRET",                   0,  0x0E);
  add("name",     "LINGER",                       0,  0x0F);
  add("name",     "BEARER",                       0,  0x10);
  add("name",     "NAPID",                        0,  0x11);
  add("name",     "COUNTRY",                      0,  0x12);
  add("name",     "NETWORK",                      0,  0x13);
  add("name",     "INTERNET",                     0,  0x14);
  add("name",     "PROXY-ID",                     0,  0x15);
  add("name",     "PROXY-PROVIDER-ID",            0,  0x16);
  add("name",     "DOMAIN",                       0,  0x17);
  add("name",     "PROVURL",                      0,  0x18);
  add("name",     "PXAUTH-TYPE",                  0,  0x19);
  add("name",     "PXAUTH-ID",                    0,  0x1A);
  add("name",     "PXAUTH-PW",                    0,  0x1B);
  add("name",     "STARTPAGE",                    0,  0x1C);
  add("name",     "BASAUTH-ID",                   0,  0x1D);
  add("name",     "BASAUTH-PW",                   0,  0x1E);
  add("name",     "PUSHENABLED",                  0,  0x1F);
  add("name",     "PXADDR",                       0,  0x20);
  add("name",     "PXADDRTYPE",                   0,  0x21);
  add("name",     "TO-NAPID",                     0,  0x22);
  add("name",     "PORTNBR",                      0,  0x23);
  add("name",     "SERVICE",                      0,  0x24);
  add("name",     "LINKSPEED",                    0,  0x25);
  add("name",     "DNLINKSPEED",                  0,  0x26);
  add("name",     "LOCAL-ADDR",                   0,  0x27);
  add("name",     "LOCAL-ADDRTYPE",               0,  0x28);
  add("name",     "CONTEXT-ALLOW",                0,  0x29);
  add("name",     "TRUST",                        0,  0x2A);
  add("name",     "MASTER",                       0,  0x2B);
  add("name",     "SID",                          0,  0x2C);
  add("name",     "SOC",                          0,  0x2D);
  add("name",     "WSP-VERSION",                  0,  0x2E);
  add("name",     "PHYSICAL-PROXY-ID",            0,  0x2F);
  add("name",     "CLIENT-ID",                    0,  0x30);
  add("name",     "DELIVERY-ERR-PDU",             0,  0x31);
  add("name",     "DELIVERY-ORDER",               0,  0x32);
  add("name",     "TRAFFIC-CLASS",                0,  0x33);
  add("name",     "MAX-SDU-SIZE",                 0,  0x34);
  add("name",     "MAX-BITRATE-UPLINK",           0,  0x35);
  add("name",     "MAX-BITRATE-DNLINK",           0,  0x36);
  add("name",     "RESIDUAL-BER",                 0,  0x37);
  add("name",     "SDU-ERROR-RATIO",              0,  0x38);
  add("name",     "TRAFFIC-HANDL-PRIO",           0,  0x39);
  add("name",     "TRANSFER-DELAY",               0,  0x3A);
  add("name",     "GUARANTEED-BITRATE-UPLINK",    0,  0x3B);
  add("name",     "GUARANTEED-BITRATE-DNLINK",    0,  0x3C);
  add("version",  "",                             0,  0x45);
  add("version",  "1.0",                          0,  0x46);
  add("type",     "",                             0,  0x50);
  add("type",     "PXLOGICAL",                    0,  0x51);
  add("type",     "PXPHYSICAL",                   0,  0x52);
  add("type",     "PORT",                         0,  0x53);
  add("type",     "VALIDITY",                     0,  0x54);
  add("type",     "NAPDEF",                       0,  0x55);
  add("type",     "BOOTSTRAP",                    0,  0x56);
/*
 *  Mark out VENDORCONFIG so if it is contained in message, parse
 *  will failed and raw data is returned.
 */
//  add("type",     "VENDORCONFIG",                 0,  0x57);
  add("type",     "CLIENTIDENTITY",               0,  0x58);
  add("type",     "PXAUTHINFO",                   0,  0x59);
  add("type",     "NAPAUTHINFO",                  0,  0x5A);

  // Code page 1
  add("name",     "",                             1,  0x05);
  add("value",    "",                             1,  0x06);
  add("name",     "NAME",                         1,  0x07);
  add("name",     "INTERNET",                     1,  0x14);
  add("name",     "STARTPAGE",                    1,  0x1C);
  add("name",     "TO-NAPID",                     1,  0x22);
  add("name",     "PORTNBR",                      1,  0x23);
  add("name",     "SERVICE",                      1,  0x24);
  add("name",     "AACCEPT",                      1,  0x2E);
  add("name",     "AAUTHDATA",                    1,  0x2F);
  add("name",     "AAUTHLEVEL",                   1,  0x30);
  add("name",     "AAUTHNAME",                    1,  0x31);
  add("name",     "AAUTHSECRET",                  1,  0x32);
  add("name",     "AAUTHTYPE",                    1,  0x33);
  add("name",     "ADDR",                         1,  0x34);
  add("name",     "ADDRTYPE",                     1,  0x35);
  add("name",     "APPID",                        1,  0x36);
  add("name",     "APROTOCOL",                    1,  0x37);
  add("name",     "PROVIDER-ID",                  1,  0x38);
  add("name",     "TO-PROXY",                     1,  0x39);
  add("name",     "URI",                          1,  0x3A);
  add("name",     "RULE",                         1,  0x3B);
  add("type",     "",                             1,  0x50);
  add("type",     "PORT",                         1,  0x53);
  add("type",     "APPLICATION",                  1,  0x55);
  add("type",     "APPADDR",                      1,  0x56);
  add("type",     "APPAUTH",                      1,  0x57);
  add("type",     "CLIENTIDENTITY",               1,  0x58);
  add("type",     "RESOURCE",                     1,  0x59);

  return names;
})();

/**
  * Value Tokens
  *
  * @see WAP-183-ProvCont-20010724-A, clause 8.3 for code page 0
  * @see OMA-WAP-TS-ProvCont-V1_1-20090421-C, clause 7.3 for code page 1
  */
const CP_VALUE_FIELDS = (function () {
  let names = {};
  function add(value, codepage, number) {
    let entry = {
      value: value,
      number: number,
    };
    if (!names[codepage]) {
      names[codepage] = {};
    }
    names[codepage][number] = entry;
  }

  // Code page 0
  add("IPV4",                             0,  0x85);
  add("IPV6",                             0,  0x86);
  add("E164",                             0,  0x87);
  add("ALPHA",                            0,  0x88);
  add("APN",                              0,  0x89);
  add("SCODE",                            0,  0x8A);
  add("TETRA-ITSI",                       0,  0x8B);
  add("MAN",                              0,  0x8C);
  add("ANALOG-MODEM",                     0,  0x90);
  add("V.120",                            0,  0x91);
  add("V.110",                            0,  0x92);
  add("X.31",                             0,  0x93);
  add("BIT-TRANSPARENT",                  0,  0x94);
  add("DIRECT-ASYNCHRONOUS-DATA-SERVICE", 0,  0x95);
  add("PAP",                              0,  0x9A);
  add("CHAP",                             0,  0x9B);
  add("HTTP-BASIC",                       0,  0x9C);
  add("HTTP-DIGEST",                      0,  0x9D);
  add("WTLS-SS",                          0,  0x9E);
  add("MD5",                              0,  0x9F);  // Added in OMA, 7.3.3
  add("GSM-USSD",                         0,  0xA2);
  add("GSM-SMS",                          0,  0xA3);
  add("ANSI-136-GUTS",                    0,  0xA4);
  add("IS-95-CDMA-SMS",                   0,  0xA5);
  add("IS-95-CDMA-CSD",                   0,  0xA6);
  add("IS-95-CDMA-PAC",                   0,  0xA7);
  add("ANSI-136-CSD",                     0,  0xA8);
  add("ANSI-136-GPRS",                    0,  0xA9);
  add("GSM-CSD",                          0,  0xAA);
  add("GSM-GPRS",                         0,  0xAB);
  add("AMPS-CDPD",                        0,  0xAC);
  add("PDC-CSD",                          0,  0xAD);
  add("PDC-PACKET",                       0,  0xAE);
  add("IDEN-SMS",                         0,  0xAF);
  add("IDEN-CSD",                         0,  0xB0);
  add("IDEN-PACKET",                      0,  0xB1);
  add("FLEX/REFLEX",                      0,  0xB2);
  add("PHS-SMS",                          0,  0xB3);
  add("PHS-CSD",                          0,  0xB4);
  add("TETRA-SDS",                        0,  0xB5);
  add("TETRA-PACKET",                     0,  0xB6);
  add("ANSI-136-GHOST",                   0,  0xB7);
  add("MOBITEX-MPAK",                     0,  0xB8);
  add("CDMA2000-1X-SIMPLE-IP",            0,  0xB9);  // Added in OMA, 7.3.4
  add("CDMA2000-1X-MOBILE-IP",            0,  0xBA);  // Added in OMA, 7.3.4
  add("AUTOBOUDING",                      0,  0xC5);
  add("CL-WSP",                           0,  0xCA);
  add("CO-WSP",                           0,  0xCB);
  add("CL-SEC-WSP",                       0,  0xCC);
  add("CO-SEC-WSP",                       0,  0xCD);
  add("CL-SEC-WTA",                       0,  0xCE);
  add("CO-SEC-WTA",                       0,  0xCF);
  add("OTA-HTTP-TO",                      0,  0xD0);  // Added in OMA, 7.3.6
  add("OTA-HTTP-TLS-TO",                  0,  0xD1);  // Added in OMA, 7.3.6
  add("OTA-HTTP-PO",                      0,  0xD2);  // Added in OMA, 7.3.6
  add("OTA-HTTP-TLS-PO",                  0,  0xD3);  // Added in OMA, 7.3.6
  add("AAA",                              0,  0xE0);  // Added in OMA, 7.3.8
  add("HA",                               0,  0xE1);  // Added in OMA, 7.3.8

  // Code page 1
  add("IPV6",                             1,  0x86);
  add("E164",                             1,  0x87);
  add("ALPHA",                            1,  0x88);
  add("APPSRV",                           1,  0x8D);
  add("OBEX",                             1,  0x8E);
  add(",",                                1,  0x90);
  add("HTTP-",                            1,  0x91);
  add("BASIC",                            1,  0x92);
  add("DIGEST",                           1,  0x93);

  return names;
})();

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-$- CpPduHelper: " + s + "\n");
  };
} else {
  debug = function (s) {};
}

this.EXPORTED_SYMBOLS = [
  // Parser
  "PduHelper",
];
