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
 * Public identifier for SL
 *
 * @see http://technical.openmobilealliance.org/tech/omna/omna-wbxml-public-docid.aspx
 */
const PUBLIC_IDENTIFIER_SL    = "-//WAPFORUM//DTD SL 1.0//EN";

this.PduHelper = {

  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param contentType [optional]
   *        Content type of incoming SL message, should be "text/vnd.wap.sl" or
   *        "application/vnd.wap.slc".
   *        Default value is "application/vnd.wap.slc".
   *
   * @return A SL message object or null in case of errors found.
   */
  parse: function parse_sl(data, contentType) {
    let msg = {};

    /**
     * Message is compressed by WBXML, decode into string.
     *
     * @see WAP-192-WBXML-20010725-A
     */
    if (!contentType || contentType === "application/vnd.wap.slc") {
      let appToken = {
        publicId: PUBLIC_IDENTIFIER_SL,
        tagToken: SL_TAG_FIELDS,
        attrToken: SL_ATTRIBUTE_FIELDS,
        globalTokenOverride: null
      }

      WBXML.PduHelper.parse(data, appToken, msg);

      msg.contentType = "text/vnd.wap.sl";
      return msg;
    }

    /**
     * Message is plain text, transform raw to string.
     */
    if (contentType === "text/vnd.wap.sl") {
      let stringData = WSP.Octet.decodeMultiple(data, data.array.length);
      msg.publicId = PUBLIC_IDENTIFIER_SL;
      msg.content = WSP.PduHelper.decodeStringContent(stringData, "UTF-8");
      msg.contentType = "text/vnd.wap.sl";
      return msg;
    }

    return null;
  },

  /**
   * @param multiStream
   *        An exsiting nsIMultiplexInputStream.
   * @param msg
   *        A SL message object.
   *
   * @return An instance of nsIMultiplexInputStream or null in case of errors.
   */
  compose: function compose_sl(multiStream, msg) {
    // Composing SL message is not supported
    return null;
  },
};

/**
 * Tag tokens
 *
 * @see WAP-168-SERVICELOAD-20010731-A, clause 9.3.1
 */
const SL_TAG_FIELDS = (function () {
  let names = {};
  function add(name, number) {
    let entry = {
      name: name,
      number: number,
    };
    names[name] = names[number] = entry;
  }

  add("sl",           0x05);

  return names;
})();

/**
 * Attribute Tokens
 *
 * @see WAP-168-SERVICELOAD-20010731-A, clause 9.3.2
 */
const SL_ATTRIBUTE_FIELDS = (function () {
  let names = {};
  function add(name, value, number) {
    let entry = {
      name: name,
      value: value,
      number: number,
    };
    names[name] = names[number] = entry;
  }

  add("action",       "execute-low",    0x05);
  add("action",       "execute-high",   0x06);
  add("action",       "cache",          0x07);
  add("href",         "",               0x08);
  add("href",         "http://",        0x09);
  add("href",         "http://www.",    0x0A);
  add("href",         "https://",       0x0B);
  add("href",         "https://www.",   0x0C);
  add("",             ".com/",          0x85);
  add("",             ".edu/",          0x86);
  add("",             ".net/",          0x87);
  add("",             ".org/",          0x88);

  return names;
})();

this.EXPORTED_SYMBOLS = [
  // Parser
  "PduHelper",
];
