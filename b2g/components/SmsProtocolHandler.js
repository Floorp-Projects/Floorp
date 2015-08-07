/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * SmsProtocolHandle.js
 *
 * This file implements the URLs for SMS
 * https://www.rfc-editor.org/rfc/rfc5724.txt
 */

"use strict";


const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import("resource:///modules/TelURIParser.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

function SmsProtocolHandler() {
}

SmsProtocolHandler.prototype = {

  scheme: "sms",
  defaultPort: -1,
  protocolFlags: Ci.nsIProtocolHandler.URI_NORELATIVE |
                 Ci.nsIProtocolHandler.URI_NOAUTH |
                 Ci.nsIProtocolHandler.URI_LOADABLE_BY_ANYONE |
                 Ci.nsIProtocolHandler.URI_DOES_NOT_RETURN_DATA,
  allowPort: function() false,

  newURI: function Proto_newURI(aSpec, aOriginCharset) {
    let uri = Cc["@mozilla.org/network/simple-uri;1"].createInstance(Ci.nsIURI);
    uri.spec = aSpec;
    return uri;
  },

  newChannel2: function Proto_newChannel2(aURI, aLoadInfo) {
    let number = TelURIParser.parseURI('sms', aURI.spec);
    let body = "";
    let query = aURI.spec.split("?")[1];

    if (query) {
      let params = query.split("&");
      params.forEach(function(aParam) {
        let [name, value] = aParam.split("=");
        if (name === "body") {
          body = decodeURIComponent(value);
        }
      })
    }

    if (number || body) {
      cpmm.sendAsyncMessage("sms-handler", {
        number: number || "",
        type: "websms/sms",
        body: body });
    }

    throw Components.results.NS_ERROR_ILLEGAL_VALUE;
  },

  newChannel: function Proto_newChannel(aURI) {
    return this.newChannel2(aURI, null);
  },

  classID: Components.ID("{81ca20cb-0dad-4e32-8566-979c8998bd73}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIProtocolHandler])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SmsProtocolHandler]);
