/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu, Ci, Cc } = require("chrome");
const JsonViewUtils = require("devtools/client/jsonview/utils");

Cu.import("resource://gre/modules/Services.jsm");

const { makeInfallible } = require("devtools/shared/DevToolsUtils");

const globalMessageManager = Cc["@mozilla.org/globalmessagemanager;1"].
  getService(Ci.nsIMessageListenerManager);
const parentProcessMessageManager = Cc["@mozilla.org/parentprocessmessagemanager;1"].
  getService(Ci.nsIMessageBroadcaster);

// Constants
const JSON_VIEW_PREF = "devtools.jsonview.enabled";

/**
 * Singleton object that represents the JSON View in-content tool.
 * It has the same lifetime as the browser. Initialization done by
 * DevTools() object from gDevTools.jsm
 */
var JsonView = {
  initialize: makeInfallible(function() {
    // Only the DevEdition has this feature available by default.
    // Users need to manually flip 'devtools.jsonview.enabled' preference
    // to have it available in other distributions.
    if (Services.prefs.getBoolPref(JSON_VIEW_PREF)) {
      // Load JSON converter module. This converter is responsible
      // for handling 'application/json' documents and converting
      // them into a simple web-app that allows easy inspection
      // of the JSON data.
      globalMessageManager.loadFrameScript(
        "resource:///modules/devtools/client/jsonview/converter-child.js",
        true);

      this.onSaveListener = this.onSave.bind(this);

      // Register for messages coming from the child process.
      parentProcessMessageManager.addMessageListener(
        "devtools:jsonview:save", this.onSaveListener);
    }
  }),

  destroy: makeInfallible(function() {
    if (this.onSaveListener) {
      parentProcessMessageManager.removeMessageListener(
        "devtools:jsonview:save", this.onSaveListener);
    }
  }),

  // Message handlers for events from child processes

  /**
   * Save JSON to a file needs to be implemented here
   * in the parent process.
   */
  onSave: function(message) {
    let value = message.data;
    let file = JsonViewUtils.getTargetFile();
    if (file) {
      JsonViewUtils.saveToFile(file, value);
    }
  }
};

// Exports from this module
module.exports.JsonView = JsonView;
