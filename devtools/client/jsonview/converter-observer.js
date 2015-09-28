/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

const {XPCOMUtils} = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

const categoryManager = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);

// Load JsonView service module (converter-child.js) as soon as required.
XPCOMUtils.defineLazyGetter(this, "JsonViewService", function() {
  const {devtools} = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
  const {JsonViewService} = devtools.require("devtools/client/jsonview/converter-child");
  return JsonViewService;
});

const JSON_TYPE = "application/json";
const GECKO_VIEWER = "Gecko-Content-Viewers";
const JSON_VIEW_PREF = "devtools.jsonview.enabled";
const GECKO_TYPE_MAPPING = "ext-to-type-mapping";

/**
 * Listen for 'devtools.jsonview.enabled' preference changes and
 * register/unregister the JSON View XPCOM service as appropriate.
 */
function ConverterObserver() {
}

ConverterObserver.prototype = {
  initialize: function() {
    this.geckoViewer = categoryManager.getCategoryEntry(GECKO_VIEWER, JSON_TYPE);

    // Only the DevEdition has this feature available by default.
    // Users need to manually flip 'devtools.jsonview.enabled' preference
    // to have it available in other distributions.
    if (this.isEnabled()) {
      this.register();
    }

    Services.prefs.addObserver(JSON_VIEW_PREF, this, false);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  observe: function(subject, topic, data) {
    switch (topic) {
      case "xpcom-shutdown":
        this.onShutdown();
        break;
      case "nsPref:changed":
        this.onPrefChanged();
        break;
    };
  },

  onShutdown: function() {
    Services.prefs.removeObserver(JSON_VIEW_PREF, observer);
    Services.obs.removeObserver(observer, "xpcom-shutdown");
  },

  onPrefChanged: function() {
    if (this.isEnabled()) {
      this.register();
    } else {
      this.unregister();
    }
  },

  register: function() {
    if (JsonViewService.register()) {
      // Delete default JSON viewer (text)
      categoryManager.deleteCategoryEntry(GECKO_VIEWER, JSON_TYPE, false);

      // Append new *.json -> application/json type mapping
      this.geckoMapping = categoryManager.addCategoryEntry(GECKO_TYPE_MAPPING,
        "json", JSON_TYPE, false, true);
    }
  },

  unregister: function() {
    if (JsonViewService.unregister()) {
      categoryManager.addCategoryEntry(GECKO_VIEWER, JSON_TYPE,
        this.geckoViewer, false, false);

      if (this.geckoMapping) {
        categoryManager.addCategoryEntry(GECKO_TYPE_MAPPING, "json",
          this.geckoMapping, false, true);
      } else {
        categoryManager.deleteCategoryEntry(GECKO_TYPE_MAPPING,
          JSON_TYPE, false)
      }
    }
  },

  isEnabled: function() {
    return Services.prefs.getBoolPref(JSON_VIEW_PREF);
  },
};

// Listen to JSON View 'enable' pref and perform dynamic
// registration or unregistration of the main application
// component.
var observer = new ConverterObserver();
observer.initialize();
