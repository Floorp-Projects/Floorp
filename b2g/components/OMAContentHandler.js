/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "cpmm", function() {
  return Cc["@mozilla.org/childprocessmessagemanager;1"]
           .getService(Ci.nsIMessageSender);
});

function debug(aMsg) {
  //dump("--*-- OMAContentHandler: " + aMsg + "\n");
}

const NS_ERROR_WONT_HANDLE_CONTENT = 0x805d0001;

function OMAContentHandler() {
}

OMAContentHandler.prototype = {
  classID: Components.ID("{a6b2ab13-9037-423a-9897-dde1081be323}"),

  _xpcom_factory: {
    createInstance: function createInstance(outer, iid) {
      if (outer != null) {
        throw Cr.NS_ERROR_NO_AGGREGATION;
      }
      return new OMAContentHandler().QueryInterface(iid);
    }
  },

  handleContent: function handleContent(aMimetype, aContext, aRequest) {
    if (!(aRequest instanceof Ci.nsIChannel)) {
      throw NS_ERROR_WONT_HANDLE_CONTENT;
    }

    let detail = {
      "type": aMimetype,
      "url": aRequest.URI.spec
    };
    cpmm.sendAsyncMessage("content-handler", detail);

    aRequest.cancel(Cr.NS_BINDING_ABORTED);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentHandler])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([OMAContentHandler]);
