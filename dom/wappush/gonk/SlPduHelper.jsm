/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

var WSP = {};
Cu.import("resource://gre/modules/WspPduHelper.jsm", WSP);
var WBXML = {};
Cu.import("resource://gre/modules/WbxmlPduHelper.jsm", WBXML);

// set to true to see debug messages
var DEBUG = WBXML.DEBUG_ALL | false;

/**
 * Public identifier for SL
 *
 * @see http://technical.openmobilealliance.org/tech/omna/omna-wbxml-public-docid.aspx
 */
const PUBLIC_IDENTIFIER_SL    = "-//WAPFORUM//DTD SL 1.0//EN";

this.PduHelper = {

  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param contentType
   *        Content type of incoming SL message, should be "text/vnd.wap.sl" or
   *        "application/vnd.wap.slc".
   *
   * @return A message object containing attribute content and contentType.
   *         |content| will contain string of decoded SL message if successfully
   *         decoded, or raw data if failed.
   *         |contentType| will be string representing corresponding type of
   *         content.
   */
  parse: function parse_sl(data, contentType) {
    // We only need content and contentType
    let msg = {
      contentType: contentType
    };

    /**
     * Message is compressed by WBXML, decode into string.
     *
     * @see WAP-192-WBXML-20010725-A
     */
    if (contentType === "application/vnd.wap.slc") {
      let appToken = {
        publicId: PUBLIC_IDENTIFIER_SL,
        tagTokenList: SL_TAG_FIELDS,
        attrTokenList: SL_ATTRIBUTE_FIELDS,
        valueTokenList: SL_VALUE_FIELDS,
        globalTokenOverride: null
      }

      try {
        let parseResult = WBXML.PduHelper.parse(data, appToken);
        msg.content = parseResult.content;
        msg.contentType = "text/vnd.wap.sl";
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
 * @see WAP-168-SERVICELOAD-20010731-A, clause 9.3.1
 */
const SL_TAG_FIELDS = (function () {
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

  add("sl",       0,  0x05);

  return names;
})();

/**
 * Attribute Tokens
 *
 * @see WAP-168-SERVICELOAD-20010731-A, clause 9.3.2
 */
const SL_ATTRIBUTE_FIELDS = (function () {
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

  add("action",       "execute-low",    0,  0x05);
  add("action",       "execute-high",   0,  0x06);
  add("action",       "cache",          0,  0x07);
  add("href",         "",               0,  0x08);
  add("href",         "http://",        0,  0x09);
  add("href",         "http://www.",    0,  0x0A);
  add("href",         "https://",       0,  0x0B);
  add("href",         "https://www.",   0,  0x0C);

  return names;
})();

const SL_VALUE_FIELDS = (function () {
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

  add(".com/",      0,  0x85);
  add(".edu/",      0,  0x86);
  add(".net/",      0,  0x87);
  add(".org/",      0,  0x88);

  return names;
})();

this.EXPORTED_SYMBOLS = [
  // Parser
  "PduHelper",
];
