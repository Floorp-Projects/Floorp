/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci } = require("chrome");
const Services = require("Services");
const { resolve } = require("promise");
const { HarUtils } = require("./har-utils.js");
const { HarBuilder } = require("./har-builder.js");

XPCOMUtils.defineLazyGetter(this, "clipboardHelper", function () {
  return Cc["@mozilla.org/widget/clipboardhelper;1"]
    .getService(Ci.nsIClipboardHelper);
});

var uid = 1;

// Helper tracer. Should be generic sharable by other modules (bug 1171927)
const trace = {
  log: function (...args) {
  }
};

/**
 * This object represents the main public API designed to access
 * Network export logic. Clients, such as the Network panel itself,
 * should use this API to export collected HTTP data from the panel.
 */
const HarExporter = {
  // Public API

  /**
   * Save collected HTTP data from the Network panel into HAR file.
   *
   * @param Object options
   *        Configuration object
   *
   * The following options are supported:
   *
   * - includeResponseBodies {Boolean}: If set to true, HTTP response bodies
   *   are also included in the HAR file (can produce significantly bigger
   *   amount of data).
   *
   * - items {Array}: List of Network requests to be exported. It is possible
   *   to use directly: NetMonitorView.RequestsMenu.items
   *
   * - jsonp {Boolean}: If set to true the export format is HARP (support
   *   for JSONP syntax).
   *
   * - jsonpCallback {String}: Default name of JSONP callback (used for
   *   HARP format).
   *
   * - compress {Boolean}: If set to true the final HAR file is zipped.
   *   This represents great disk-space optimization.
   *
   * - defaultFileName {String}: Default name of the target HAR file.
   *   The default file name supports formatters, see:
   *   https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toLocaleFormat
   *
   * - defaultLogDir {String}: Default log directory for automated logs.
   *
   * - id {String}: ID of the page (used in the HAR file).
   *
   * - title {String}: Title of the page (used in the HAR file).
   *
   * - forceExport {Boolean}: The result HAR file is created even if
   *   there are no HTTP entries.
   */
  save: function (options) {
    // Set default options related to save operation.
    options.defaultFileName = Services.prefs.getCharPref(
      "devtools.netmonitor.har.defaultFileName");
    options.compress = Services.prefs.getBoolPref(
      "devtools.netmonitor.har.compress");

    // Get target file for exported data. Bail out, if the user
    // presses cancel.
    let file = HarUtils.getTargetFile(options.defaultFileName,
      options.jsonp, options.compress);

    if (!file) {
      return resolve();
    }

    trace.log("HarExporter.save; " + options.defaultFileName, options);

    return this.fetchHarData(options).then(jsonString => {
      if (!HarUtils.saveToFile(file, jsonString, options.compress)) {
        let msg = "Failed to save HAR file at: " + options.defaultFileName;
        console.error(msg);
      }
      return jsonString;
    });
  },

  /**
   * Copy HAR string into the clipboard.
   *
   * @param Object options
   *        Configuration object, see save() for detailed description.
   */
  copy: function (options) {
    return this.fetchHarData(options).then(jsonString => {
      clipboardHelper.copyString(jsonString);
      return jsonString;
    });
  },

  // Helpers

  fetchHarData: function (options) {
    // Generate page ID
    options.id = options.id || uid++;

    // Set default generic HAR export options.
    options.jsonp = options.jsonp ||
      Services.prefs.getBoolPref("devtools.netmonitor.har.jsonp");
    options.includeResponseBodies = options.includeResponseBodies ||
      Services.prefs.getBoolPref(
        "devtools.netmonitor.har.includeResponseBodies");
    options.jsonpCallback = options.jsonpCallback ||
      Services.prefs.getCharPref("devtools.netmonitor.har.jsonpCallback");
    options.forceExport = options.forceExport ||
      Services.prefs.getBoolPref("devtools.netmonitor.har.forceExport");

    // Build HAR object.
    return this.buildHarData(options).then(har => {
      // Do not export an empty HAR file, unless the user
      // explicitly says so (using the forceExport option).
      if (!har.log.entries.length && !options.forceExport) {
        return resolve();
      }

      let jsonString = this.stringify(har);
      if (!jsonString) {
        return resolve();
      }

      // If JSONP is wanted, wrap the string in a function call
      if (options.jsonp) {
        // This callback name is also used in HAR Viewer by default.
        // http://www.softwareishard.com/har/viewer/
        let callbackName = options.jsonpCallback || "onInputData";
        jsonString = callbackName + "(" + jsonString + ");";
      }

      return jsonString;
    }).then(null, function onError(err) {
      console.error(err);
    });
  },

  /**
   * Build HAR data object. This object contains all HTTP data
   * collected by the Network panel. The process is asynchronous
   * since it can involve additional RDP communication (e.g. resolving
   * long strings).
   */
  buildHarData: function (options) {
    // Build HAR object from collected data.
    let builder = new HarBuilder(options);
    return builder.build();
  },

  /**
   * Build JSON string from the HAR data object.
   */
  stringify: function (har) {
    if (!har) {
      return null;
    }

    try {
      return JSON.stringify(har, null, "  ");
    } catch (err) {
      console.error(err);
      return undefined;
    }
  },
};

// Exports from this module
exports.HarExporter = HarExporter;
