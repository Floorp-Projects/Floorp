/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const JSZip = require("devtools/client/shared/vendor/jszip");
const clipboardHelper = require("devtools/shared/platform/clipboard");
const { HarUtils } = require("devtools/client/netmonitor/src/har/har-utils");
const {
  HarBuilder,
} = require("devtools/client/netmonitor/src/har/har-builder");

var uid = 1;

// Helper tracer. Should be generic sharable by other modules (bug 1171927)
const trace = {
  log(...args) {},
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
    const defaultFileName = Services.prefs.getCharPref(
      "devtools.netmonitor.har.defaultFileName"
    );
    const compress = Services.prefs.getBoolPref(
      "devtools.netmonitor.har.compress"
    );

    trace.log("HarExporter.save; " + defaultFileName, options);

    let data = await this.fetchHarData(options);

    const tabTarget = options.connector.getTabTarget();
    const host = new URL(tabTarget.url);

    const fileName = HarUtils.getHarFileName(
      defaultFileName,
      options.jsonp,
      compress,
      host.hostname
    );

    if (compress) {
      data = await JSZip()
        .file(fileName, data)
        .generateAsync({
          compression: "DEFLATE",
          platform: Services.appinfo.OS === "WINNT" ? "DOS" : "UNIX",
          type: "uint8array",
        });
    } else {
      data = new TextEncoder().encode(data);
    }

    DevToolsUtils.saveAs(window, data, fileName);
  },

  /**
   * Copy HAR string into the clipboard.
   *
   * @param Object options
   *        Configuration object, see save() for detailed description.
   */
  copy(options) {
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
  getHar(options) {
    return this.fetchHarData(options).then(data => {
      return data ? JSON.parse(data) : null;
    });
  },

  // Helpers

  fetchHarData(options) {
    // Generate page ID
    options.id = options.id || uid++;

    // Set default generic HAR export options.
    if (typeof options.jsonp != "boolean") {
      options.jsonp = Services.prefs.getBoolPref(
        "devtools.netmonitor.har.jsonp"
      );
    }
    if (typeof options.includeResponseBodies != "boolean") {
      options.includeResponseBodies = Services.prefs.getBoolPref(
        "devtools.netmonitor.har.includeResponseBodies"
      );
    }
    if (typeof options.jsonpCallback != "boolean") {
      options.jsonpCallback = Services.prefs.getCharPref(
        "devtools.netmonitor.har.jsonpCallback"
      );
    }
    if (typeof options.forceExport != "boolean") {
      options.forceExport = Services.prefs.getBoolPref(
        "devtools.netmonitor.har.forceExport"
      );
    }

    // Build HAR object.
    return this.buildHarData(options)
      .then(har => {
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
          const callbackName = options.jsonpCallback || "onInputData";
          jsonString = callbackName + "(" + jsonString + ");";
        }

        return jsonString;
      })
      .catch(function onError(err) {
        console.error(err);
      });
  },

  /**
   * Build HAR data object. This object contains all HTTP data
   * collected by the Network panel. The process is asynchronous
   * since it can involve additional RDP communication (e.g. resolving
   * long strings).
   */
  async buildHarData(options) {
    const { connector } = options;
    const { getTabTarget } = connector;
    const { title } = getTabTarget();

    // Disconnect from redux actions/store.
    connector.enableActions(false);

    options = {
      ...options,
      title,
      getString: connector.getLongString,
      getTimingMarker: connector.getTimingMarker,
      requestData: connector.requestData,
    };

    // Build HAR object from collected data.
    const builder = new HarBuilder(options);
    const result = await builder.build();

    // Connect to redux actions again.
    connector.enableActions(true);

    return result;
  },

  /**
   * Build JSON string from the HAR data object.
   */
  stringify(har) {
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
