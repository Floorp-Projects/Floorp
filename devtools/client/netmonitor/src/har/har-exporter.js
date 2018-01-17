/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const FileSaver = require("devtools/client/shared/file-saver");
const JSZip = require("devtools/client/shared/vendor/jszip");
const clipboardHelper = require("devtools/shared/platform/clipboard");
const { HarBuilder } = require("./har-builder");

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
   * - items {Array}: List of Network requests to be exported.
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
   *   The default file name supports the format specifier %date to output the
   *   current date/time.
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
  async save(options) {
    // Set default options related to save operation.
    let defaultFileName = Services.prefs.getCharPref(
      "devtools.netmonitor.har.defaultFileName");
    let compress = Services.prefs.getBoolPref(
      "devtools.netmonitor.har.compress");

    trace.log("HarExporter.save; " + defaultFileName, options);

    let data = await this.fetchHarData(options);
    let fileName = this.getHarFileName(defaultFileName, options.jsonp, compress);

    if (compress) {
      data = await JSZip().file(fileName, data).generateAsync({
        compression: "DEFLATE",
        platform: Services.appinfo.OS === "WINNT" ? "DOS" : "UNIX",
        type: "blob",
      });
    }

    fileName = `${fileName}${compress ? ".zip" : ""}`;
    let blob = compress ? data : new Blob([data], { type: "application/json" });

    FileSaver.saveAs(blob, fileName, document);
  },

  formatDate(date) {
    let year = String(date.getFullYear() % 100).padStart(2, "0");
    let month = String(date.getMonth() + 1).padStart(2, "0");
    let day = String(date.getDate()).padStart(2, "0");
    let hour = String(date.getHours()).padStart(2, "0");
    let minutes = String(date.getMinutes()).padStart(2, "0");
    let seconds = String(date.getSeconds()).padStart(2, "0");

    return `${year}-${month}-${day} ${hour}-${minutes}-${seconds}`;
  },

  getHarFileName(defaultFileName, jsonp, compress) {
    let name = defaultFileName.replace(/%date/g, this.formatDate(new Date()));
    name = name.replace(/\:/gm, "-", "");
    name = name.replace(/\//gm, "_", "");
    return `${name}.${jsonp ? "harp" : "har"}`;
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

  /**
   * Get HAR data as JSON object.
   *
   * @param Object options
   *        Configuration object, see save() for detailed description.
   */
  getHar: function (options) {
    return this.fetchHarData(options).then(JSON.parse);
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
        return Promise.resolve();
      }

      let jsonString = this.stringify(har);
      if (!jsonString) {
        return Promise.resolve();
      }

      // If JSONP is wanted, wrap the string in a function call
      if (options.jsonp) {
        // This callback name is also used in HAR Viewer by default.
        // http://www.softwareishard.com/har/viewer/
        let callbackName = options.jsonpCallback || "onInputData";
        jsonString = callbackName + "(" + jsonString + ");";
      }

      return jsonString;
    }).catch(function onError(err) {
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
