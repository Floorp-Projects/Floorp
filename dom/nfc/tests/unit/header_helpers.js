/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let NFC_CONSTS = {};
let subscriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                        .getService(Ci.mozIJSSubScriptLoader);

Cu.import("resource://gre/modules/nfc_consts.js", NFC_CONSTS);
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

function waitAsyncMessage(msg, callback) {
  let handler = {
    receiveMessage: function (message) {
      if (message.name !== msg) {
        return;
      }
      cpmm.removeMessageListener(msg, handler);
      callback(message);
    }
  };

  cpmm.addMessageListener(msg, handler);
}

function sendAsyncMessage(msg, rsp, payload, callback) {
  waitAsyncMessage(rsp, callback);
  cpmm.sendAsyncMessage(msg, payload);
}

function sendSyncMessage(msg, payload) {
  return cpmm.sendSyncMessage(msg, payload);
}

