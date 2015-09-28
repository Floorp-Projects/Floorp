/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, components} = require("chrome");
const xpcom = require("sdk/platform/xpcom");
const {Unknown} = require("sdk/platform/xpcom");
const {Class} = require("sdk/core/heritage");

const categoryManager = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);

loader.lazyRequireGetter(this, "NetworkHelper",
                               "devtools/shared/webconsole/network-helper");

// Constants
const JSON_TYPE = "application/json";
const CONTRACT_ID = "@mozilla.org/devtools/jsonview-sniffer;1";
const CLASS_ID = "{4148c488-dca1-49fc-a621-2a0097a62422}";
const JSON_VIEW_MIME_TYPE = "application/vnd.mozilla.json.view";
const JSON_VIEW_TYPE = "JSON View";
const CONTENT_SNIFFER_CATEGORY = "net-content-sniffers";

/**
 * This component represents a sniffer (implements nsIContentSniffer
 * interface) responsible for changing top level 'application/json'
 * document types to: 'application/vnd.mozilla.json.view'.
 *
 * This internal type is consequently rendered by JSON View component
 * that represents the JSON through a viewer interface.
 */
var Sniffer = Class({
  extends: Unknown,

  interfaces: [
    "nsIContentSniffer",
  ],

  get wrappedJSObject() this,

  getMIMETypeFromContent: function(aRequest, aData, aLength) {
    // JSON View is enabled only for top level loads only.
    if (!NetworkHelper.isTopLevelLoad(aRequest)) {
      return "";
    }

    if (aRequest instanceof Ci.nsIChannel) {
      try {
        if (aRequest.contentDisposition == Ci.nsIChannel.DISPOSITION_ATTACHMENT) {
          return "";
        }
      } catch (e) {
        // Channel doesn't support content dispositions
      }

      // Check the response content type and if it's application/json
      // change it to new internal type consumed by JSON View.
      if (aRequest.contentType == JSON_TYPE) {
        return JSON_VIEW_MIME_TYPE;
      }
    }

    return "";
  }
});

var service = xpcom.Service({
  id: components.ID(CLASS_ID),
  contract: CONTRACT_ID,
  Component: Sniffer,
  register: false,
  unregister: false
});

function register() {
  if (!xpcom.isRegistered(service)) {
    xpcom.register(service);
    categoryManager.addCategoryEntry(CONTENT_SNIFFER_CATEGORY, JSON_VIEW_TYPE,
      CONTRACT_ID, false, false);
    return true;
  }

  return false;
}

function unregister() {
  if (xpcom.isRegistered(service)) {
    categoryManager.deleteCategoryEntry(CONTENT_SNIFFER_CATEGORY,
      JSON_VIEW_TYPE, false)
    xpcom.unregister(service);
    return true;
  }
  return false;
}

exports.JsonViewSniffer = {
  register: register,
  unregister: unregister
}
