/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu, Ci, Cc} = require("chrome");
const JsonViewUtils = require("devtools/client/jsonview/utils");

// Constants
const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});
const {makeInfallible} = require("devtools/shared/DevToolsUtils");

/**
 * Singleton object that represents the JSON View in-content tool.
 * It has the same lifetime as the browser. Initialization done by
 * DevTools() object from gDevTools.jsm
 */
var JsonView = {
  initialize: makeInfallible(function() {
    // Load JSON converter module. This converter is responsible
    // for handling 'application/json' documents and converting
    // them into a simple web-app that allows easy inspection
    // of the JSON data.
    Services.ppmm.loadProcessScript(
      "resource:///modules/devtools/client/jsonview/converter-observer.js",
      true);

    this.onSaveListener = this.onSave.bind(this);

    // Register for messages coming from the child process.
    Services.ppmm.addMessageListener(
      "devtools:jsonview:save", this.onSaveListener);
  }),

  destroy: makeInfallible(function() {
    Services.ppmm.removeMessageListener(
      "devtools:jsonview:save", this.onSaveListener);
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
