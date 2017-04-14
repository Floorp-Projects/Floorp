/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cm = Components.manager;
const Cr = Components.results;
const Cu = Components.utils;

const {XPCOMUtils} = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

// Load devtools module lazily.
XPCOMUtils.defineLazyGetter(this, "devtools", function () {
  const {devtools} = Cu.import("resource://devtools/shared/Loader.jsm", {});
  return devtools;
});

// Load JsonView services lazily.
XPCOMUtils.defineLazyGetter(this, "JsonViewService", function () {
  const {JsonViewService} = devtools.require("devtools/client/jsonview/converter-child");
  return JsonViewService;
});

// Constants
const JSON_VIEW_PREF = "devtools.jsonview.enabled";
const JSON_VIEW_MIME_TYPE = "application/vnd.mozilla.json.view";
const JSON_VIEW_CONTRACT_ID = "@mozilla.org/streamconv;1?from=" +
  JSON_VIEW_MIME_TYPE + "&to=*/*";
const JSON_VIEW_CLASS_ID = Components.ID("{d8c9acee-dec5-11e4-8c75-1681e6b88ec1}");
const JSON_VIEW_CLASS_DESCRIPTION = "JSONView converter";

const JSON_SNIFFER_CONTRACT_ID = "@mozilla.org/devtools/jsonview-sniffer;1";
const JSON_SNIFFER_CLASS_ID = Components.ID("{4148c488-dca1-49fc-a621-2a0097a62422}");
const JSON_SNIFFER_CLASS_DESCRIPTION = "JSONView content sniffer";
const JSON_VIEW_TYPE = "JSON View";
const CONTENT_SNIFFER_CATEGORY = "net-content-sniffers";

/**
 * This component represents a sniffer (implements nsIContentSniffer
 * interface) responsible for changing top level 'application/json'
 * document types to: 'application/vnd.mozilla.json.view'.
 *
 * This internal type is consequently rendered by JSON View component
 * that represents the JSON through a viewer interface.
 *
 * This is done in the .js file rather than a .jsm to avoid creating
 * a compartment at startup when no JSON is being viewed.
 */
function JsonViewSniffer() {}

JsonViewSniffer.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentSniffer]),

  get wrappedJSObject() {
    return this;
  },

  isTopLevelLoad: function (request) {
    let loadInfo = request.loadInfo;
    if (loadInfo && loadInfo.isTopLevelLoad) {
      return (request.loadFlags & Ci.nsIChannel.LOAD_DOCUMENT_URI);
    }
    return false;
  },

  getMIMETypeFromContent: function (request, data, length) {
    if (request instanceof Ci.nsIChannel) {
      // JSON View is enabled only for top level loads only.
      if (!this.isTopLevelLoad(request)) {
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

      // Check the response content type and if it's a valid type
      // such as application/json or application/manifest+json
      // change it to new internal type consumed by JSON View.
      const JSON_TYPES = ["application/json", "application/manifest+json"];
      if (JSON_TYPES.includes(request.contentType)) {
        return JSON_VIEW_MIME_TYPE;
      }
    }

    return "";
  }
};

/*
 * Create instances of the JSON view sniffer.
 */
const JsonSnifferFactory = {
  createInstance: function (outer, iid) {
    if (outer) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return new JsonViewSniffer();
  }
};

/*
 * Create instances of the JSON view converter.
 * This is done in the .js file rather than a .jsm to avoid creating
 * a compartment at startup when no JSON is being viewed.
 */
const JsonViewFactory = {
  createInstance: function (outer, iid) {
    if (outer) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return JsonViewService.createInstance();
  }
};

/**
 * Listen for 'devtools.jsonview.enabled' preference changes and
 * register/unregister the JSON View XPCOM services as appropriate.
 */
function ConverterObserver() {
}

ConverterObserver.prototype = {
  initialize: function () {
    // Only the DevEdition has this feature available by default.
    // Users need to manually flip 'devtools.jsonview.enabled' preference
    // to have it available in other distributions.
    if (this.isEnabled()) {
      this.register();
    }

    Services.prefs.addObserver(JSON_VIEW_PREF, this, false);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  observe: function (subject, topic, data) {
    switch (topic) {
      case "xpcom-shutdown":
        this.onShutdown();
        break;
      case "nsPref:changed":
        this.onPrefChanged();
        break;
    }
  },

  onShutdown: function () {
    Services.prefs.removeObserver(JSON_VIEW_PREF, observer);
    Services.obs.removeObserver(observer, "xpcom-shutdown");
  },

  onPrefChanged: function () {
    if (this.isEnabled()) {
      this.register();
    } else {
      this.unregister();
    }
  },

  register: function () {
    const registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);

    if (!registrar.isCIDRegistered(JSON_SNIFFER_CLASS_ID)) {
      registrar.registerFactory(JSON_SNIFFER_CLASS_ID,
        JSON_SNIFFER_CLASS_DESCRIPTION,
        JSON_SNIFFER_CONTRACT_ID,
        JsonSnifferFactory);
      const categoryManager = Cc["@mozilla.org/categorymanager;1"]
        .getService(Ci.nsICategoryManager);
      categoryManager.addCategoryEntry(CONTENT_SNIFFER_CATEGORY, JSON_VIEW_TYPE,
        JSON_SNIFFER_CONTRACT_ID, false, false);
    }

    if (!registrar.isCIDRegistered(JSON_VIEW_CLASS_ID)) {
      registrar.registerFactory(JSON_VIEW_CLASS_ID,
        JSON_VIEW_CLASS_DESCRIPTION,
        JSON_VIEW_CONTRACT_ID,
        JsonViewFactory);
    }
  },

  unregister: function () {
    const registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);

    if (registrar.isCIDRegistered(JSON_SNIFFER_CLASS_ID)) {
      registrar.unregisterFactory(JSON_SNIFFER_CLASS_ID, JsonSnifferFactory);
      const categoryManager = Cc["@mozilla.org/categorymanager;1"]
        .getService(Ci.nsICategoryManager);
      categoryManager.deleteCategoryEntry(CONTENT_SNIFFER_CATEGORY,
        JSON_VIEW_TYPE, false);
    }

    if (registrar.isCIDRegistered(JSON_VIEW_CLASS_ID)) {
      registrar.unregisterFactory(JSON_VIEW_CLASS_ID, JsonViewFactory);
    }
  },

  isEnabled: function () {
    return Services.prefs.getBoolPref(JSON_VIEW_PREF);
  },
};

// Listen to JSON View 'enable' pref and perform dynamic
// registration or unregistration of the main application
// component.
var observer = new ConverterObserver();
observer.initialize();
