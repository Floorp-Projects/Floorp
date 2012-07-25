/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const PDF_CONTENT_TYPE = "application/pdf";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function log(aMsg) {
  let msg = "ContentHandler.js: " + (aMsg.join ? aMsg.join("") : aMsg);
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService)
                                     .logStringMessage(msg);
  dump(msg + "\n");
}

const NS_ERROR_WONT_HANDLE_CONTENT = 0x805d0001;
function ContentHandler() {
}

ContentHandler.prototype = {
  handleContent: function handleContent(aMimetype, aContext, aRequest) {
    if (aMimetype != PDF_CONTENT_TYPE)
      throw NS_ERROR_WONT_HANDLE_CONTENT;

    if (!(aRequest instanceof Ci.nsIChannel))
      throw NS_ERROR_WONT_HANDLE_CONTENT;

    let detail = {
      "type": aMimetype,
      "url": aRequest.URI.spec
    };
    Services.obs.notifyObservers(this, "content-handler", JSON.stringify(detail)); 

    aRequest.cancel(Cr.NS_BINDING_ABORTED);
  },

  classID: Components.ID("{d18d0216-d50c-11e1-ba54-efb18d0ef0ac}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentHandler])
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentHandler]);
