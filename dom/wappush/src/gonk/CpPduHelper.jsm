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
  * @see WAP-183-ProvCont-20010724-A, clause 8.1
  */
const CP_TAG_FIELDS = (function () {
  let names = {};
  function add(name, number) {
    let entry = {
      name: name,
      number: number,
    };
    names[number] = entry;
  }

  add("wap-provisioningdoc", 0x05);
  add("characteristic",      0x06);
  add("parm",                0x07);

  return names;
})();

/**
  * Attribute Tokens
  *
  * @see WAP-183-ProvCont-20010724-A, clause 8.2
  */
const CP_ATTRIBUTE_FIELDS = (function () {
  let names = {};
  function add(name, value, number) {
    let entry = {
      name: name,
      value: value,
      number: number,
    };
    names[number] = entry;
  }

  add("name",     "",                                 0x05);
  add("value",    "",                                 0x06);
  add("name",     "NAME",                             0x07);
  add("name",     "NAP-ADDRESS",                      0x08);
  add("name",     "NAP-ADDRTYPE",                     0x09);
  add("name",     "CALLTYPE",                         0x0A);
  add("name",     "VALIDUNTIL",                       0x0B);
  add("name",     "AUTHTYPE",                         0x0C);
  add("name",     "AUTHNAME",                         0x0D);
  add("name",     "AUTHSECRET",                       0x0E);
  add("name",     "LINGER",                           0x0F);
  add("name",     "BEARER",                           0x10);
  add("name",     "NAPID",                            0x11);
  add("name",     "COUNTRY",                          0x12);
  add("name",     "NETWORK",                          0x13);
  add("name",     "INTERNET",                         0x14);
  add("name",     "PROXY-ID",                         0x15);
  add("name",     "PROXY-PROVIDER-ID",                0x16);
  add("name",     "DOMAIN",                           0x17);
  add("name",     "PROVURL",                          0x18);
  add("name",     "PXAUTH-TYPE",                      0x19);
  add("name",     "PXAUTH-ID",                        0x1A);
  add("name",     "PXAUTH-PW",                        0x1B);
  add("name",     "STARTPAGE",                        0x1C);
  add("name",     "BASAUTH-ID",                       0x1D);
  add("name",     "BASAUTH-PW",                       0x1E);
  add("name",     "PUSHENABLED",                      0x1F);
  add("name",     "PXADDR",                           0x20);
  add("name",     "PXADDRTYPE",                       0x21);
  add("name",     "TO-NAPID",                         0x22);
  add("name",     "PORTNBR",                          0x23);
  add("name",     "SERVICE",                          0x24);
  add("name",     "LINKSPEED",                        0x25);
  add("name",     "DNLINKSPEED",                      0x26);
  add("name",     "LOCAL-ADDR",                       0x27);
  add("name",     "LOCAL-ADDRTYPE",                   0x28);
  add("name",     "CONTEXT-ALLOW",                    0x29);
  add("name",     "TRUST",                            0x2A);
  add("name",     "MASTER",                           0x2B);
  add("name",     "SID",                              0x2C);
  add("name",     "SOC",                              0x2D);
  add("name",     "WSP-VERSION",                      0x2E);
  add("name",     "PHYSICAL-PROXY-ID",                0x2F);
  add("name",     "CLIENT-ID",                        0x30);
  add("name",     "DELIVERY-ERR-PDU",                 0x31);
  add("name",     "DELIVERY-ORDER",                   0x32);
  add("name",     "TRAFFIC-CLASS",                    0x33);
  add("name",     "MAX-SDU-SIZE",                     0x34);
  add("name",     "MAX-BITRATE-UPLINK",               0x35);
  add("name",     "MAX-BITRATE-DNLINK",               0x36);
  add("name",     "RESIDUAL-BER",                     0x37);
  add("name",     "SDU-ERROR-RATIO",                  0x38);
  add("name",     "TRAFFIC-HANDL-PRIO",               0x39);
  add("name",     "TRANSFER-DELAY",                   0x3A);
  add("name",     "GUARANTEED-BITRATE-UPLINK",        0x3B);
  add("name",     "GUARANTEED-BITRATE-DNLINK",        0x3C);
  add("version",  "",                                 0x45);
  add("version",  "1.0",                              0x46);
  add("type",     "",                                 0x50);
  add("type",     "PXLOGICAL",                        0x51);
  add("type",     "PXPHYSICAL",                       0x52);
  add("type",     "PORT",                             0x53);
  add("type",     "VALIDITY",                         0x54);
  add("type",     "NAPDEF",                           0x55);
  add("type",     "BOOTSTRAP",                        0x56);
/*
 *  Mark out VENDORCONFIG so if it is contained in message, parse
 *  will failed and raw data is returned.
 */
//  add("type",     "VENDORCONFIG",                     0x57);
  add("type",     "CLIENTIDENTITY",                   0x58);
  add("type",     "PXAUTHINFO",                       0x59);
  add("type",     "NAPAUTHINFO",                      0x5A);

  return names;
})();

const CP_VALUE_FIELDS = (function () {
  let names = {};
  function add(value, number) {
    let entry = {
      value: value,
      number: number,
    };
    names[number] = entry;
  }

  add("IPV4",                             0x85);
  add("IPV6",                             0x86);
  add("E164",                             0x87);
  add("ALPHA",                            0x88);
  add("APN",                              0x89);
  add("SCODE",                            0x8A);
  add("TETRA-ITSI",                       0x8B);
  add("MAN",                              0x8C);
  add("ANALOG-MODEM",                     0x90);
  add("V.120",                            0x91);
  add("V.110",                            0x92);
  add("X.31",                             0x93);
  add("BIT-TRANSPARENT",                  0x94);
  add("DIRECT-ASYNCHRONOUS-DATA-SERVICE", 0x95);
  add("PAP",                              0x9A);
  add("CHAP",                             0x9B);
  add("HTTP-BASIC",                       0x9C);
  add("HTTP-DIGEST",                      0x9D);
  add("WTLS-SS",                          0x9E);
  add("GSM-USSD",                         0xA2);
  add("GSM-SMS",                          0xA3);
  add("ANSI-136-GUTS",                    0xA4);
  add("IS-95-CDMA-SMS",                   0xA5);
  add("IS-95-CDMA-CSD",                   0xA6);
  add("IS-95-CDMA-PAC",                   0xA7);
  add("ANSI-136-CSD",                     0xA8);
  add("ANSI-136-GPRS",                    0xA9);
  add("GSM-CSD",                          0xAA);
  add("GSM-GPRS",                         0xAB);
  add("AMPS-CDPD",                        0xAC);
  add("PDC-CSD",                          0xAD);
  add("PDC-PACKET",                       0xAE);
  add("IDEN-SMS",                         0xAF);
  add("IDEN-CSD",                         0xB0);
  add("IDEN-PACKET",                      0xB1);
  add("FLEX/REFLEX",                      0xB2);
  add("PHS-SMS",                          0xB3);
  add("PHS-CSD",                          0xB4);
  add("TETRA-SDS",                        0xB5);
  add("TETRA-PACKET",                     0xB6);
  add("ANSI-136-GHOST",                   0xB7);
  add("MOBITEX-MPAK",                     0xB8);
  add("AUTOBOUDING",                      0xC5);
  add("CL-WSP",                           0xCA);
  add("CO-WSP",                           0xCB);
  add("CL-SEC-WSP",                       0xCC);
  add("CO-SEC-WSP",                       0xCD);
  add("CL-SEC-WTA",                       0xCE);
  add("CO-SEC-WTA",                       0xCF);

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
