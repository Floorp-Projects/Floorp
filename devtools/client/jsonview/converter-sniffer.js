/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu, Cm, Cr, components} = require("chrome");
const registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});

const categoryManager = Cc["@mozilla.org/categorymanager;1"]
  .getService(Ci.nsICategoryManager);

// Constants
const JSON_TYPE = "application/json";
const CONTRACT_ID = "@mozilla.org/devtools/jsonview-sniffer;1";
const CLASS_ID = components.ID("{4148c488-dca1-49fc-a621-2a0097a62422}");
const CLASS_DESCRIPTION = "JSONView content sniffer";
const JSON_VIEW_MIME_TYPE = "application/vnd.mozilla.json.view";
const JSON_VIEW_TYPE = "JSON View";
const CONTENT_SNIFFER_CATEGORY = "net-content-sniffers";

function isTopLevelLoad(request) {
  let loadInfo = request.loadInfo;
  if (loadInfo && loadInfo.isTopLevelLoad) {
    return (request.loadFlags & Ci.nsIChannel.LOAD_DOCUMENT_URI);
  }
  return false;
}

/**
 * This component represents a sniffer (implements nsIContentSniffer
 * interface) responsible for changing top level 'application/json'
 * document types to: 'application/vnd.mozilla.json.view'.
 *
 * This internal type is consequently rendered by JSON View component
 * that represents the JSON through a viewer interface.
 */
function Sniffer() {}

Sniffer.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentSniffer]),

  get wrappedJSObject() {
    return this;
  },

  getMIMETypeFromContent: function (request, data, length) {
    if (request instanceof Ci.nsIChannel) {
      // JSON View is enabled only for top level loads only.
      if (!isTopLevelLoad(request)) {
        return "";
      }
      try {
        if (request.contentDisposition ==
          Ci.nsIChannel.DISPOSITION_ATTACHMENT) {
          return "";
        }
      } catch (e) {
        // Channel doesn't support content dispositions
      }

      // Check the response content type and if it's application/json
      // change it to new internal type consumed by JSON View.
      if (request.contentType == JSON_TYPE) {
        return JSON_VIEW_MIME_TYPE;
      }
    }

    return "";
  }
};

const Factory = {
  createInstance: function (outer, iid) {
    if (outer) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return new Sniffer();
  }
};

function register() {
  if (!registrar.isCIDRegistered(CLASS_ID)) {
    registrar.registerFactory(CLASS_ID,
      CLASS_DESCRIPTION,
      CONTRACT_ID,
      Factory);
    categoryManager.addCategoryEntry(CONTENT_SNIFFER_CATEGORY, JSON_VIEW_TYPE,
      CONTRACT_ID, false, false);
    return true;
  }

  return false;
}

function unregister() {
  if (registrar.isCIDRegistered(CLASS_ID)) {
    registrar.unregisterFactory(CLASS_ID, Factory);
    categoryManager.deleteCategoryEntry(CONTENT_SNIFFER_CATEGORY,
      JSON_VIEW_TYPE, false);
    return true;
  }
  return false;
}

exports.JsonViewSniffer = {
  register: register,
  unregister: unregister
};
