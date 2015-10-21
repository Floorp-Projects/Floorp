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

// Load devtools module lazily.
XPCOMUtils.defineLazyGetter(this, "devtools", function() {
  const {devtools} = Cu.import("resource://devtools/shared/Loader.jsm", {});
  return devtools;
});

// Load JsonView services lazily.
XPCOMUtils.defineLazyGetter(this, "JsonViewService", function() {
  const {JsonViewService} = devtools.require("devtools/client/jsonview/converter-child");
  return JsonViewService;
});

XPCOMUtils.defineLazyGetter(this, "JsonViewSniffer", function() {
  const {JsonViewSniffer} = devtools.require("devtools/client/jsonview/converter-sniffer");
  return JsonViewSniffer;
});

// Constants
const JSON_VIEW_PREF = "devtools.jsonview.enabled";

/**
 * Listen for 'devtools.jsonview.enabled' preference changes and
 * register/unregister the JSON View XPCOM services as appropriate.
 */
function ConverterObserver() {
}

ConverterObserver.prototype = {
  initialize: function() {
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
    JsonViewSniffer.register();
    JsonViewService.register();
  },

  unregister: function() {
    JsonViewSniffer.unregister();
    JsonViewService.unregister();
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
