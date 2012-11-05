/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

function MailtoProtocolHandler() {
}

MailtoProtocolHandler.prototype = {

  scheme: "mailto",
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

  newChannel: function Proto_newChannel(aURI) {
    cpmm.sendAsyncMessage("mail-handler", {
      URI: aURI.spec,
      type: "mail" });

    throw Components.results.NS_ERROR_ILLEGAL_VALUE;
  },

  classID: Components.ID("{50777e53-0331-4366-a191-900999be386c}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIProtocolHandler])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MailtoProtocolHandler]);
