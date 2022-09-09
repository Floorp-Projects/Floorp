/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const {
  HarCollector,
} = require("devtools/client/netmonitor/src/har/har-collector");
const {
  HarExporter,
} = require("devtools/client/netmonitor/src/har/har-exporter");
const { HarUtils } = require("devtools/client/netmonitor/src/har/har-utils");
const {
  getLongStringFullText,
} = require("devtools/client/shared/string-utils");

const prefDomain = "devtools.netmonitor.har.";

// Helper tracer. Should be generic sharable by other modules (bug 1171927)
const trace = {
  log(...args) {},
};

/**
 * This object is responsible for automated HAR export. It listens
 * for Network activity, collects all HTTP data and triggers HAR
 * export when the page is loaded.
 *
 * The user needs to enable the following preference to make the
 * auto-export work: devtools.netmonitor.har.enableAutoExportToFile
 *
 * HAR files are stored within directory that is specified in this
 * preference: devtools.netmonitor.har.defaultLogDir
 *
 * If the default log directory preference isn't set the following
 * directory is used by default: <profile>/har/logs
 */
function HarAutomation() {}

HarAutomation.prototype = {
  // Initialization

  async initialize(toolbox) {
    this.toolbox = toolbox;
    this.commands = toolbox.commands;

    await this.startMonitoring();
  },

  destroy() {
    if (this.collector) {
      this.collector.stop();
    }

    if (this.tabWatcher) {
      this.tabWatcher.disconnect();
    }
  },

  // Automation

  async startMonitoring() {
    await this.toolbox.resourceCommand.watchResources(
      [this.toolbox.resourceCommand.TYPES.DOCUMENT_EVENT],
      {
        onAvailable: resources => {
          // Only consider top level document, and ignore remote iframes top document
          if (
            resources.find(
              r => r.name == "will-navigate" && r.targetFront.isTopLevel
            )
          ) {
            this.pageLoadBegin();
          }
          if (
            resources.find(
              r => r.name == "dom-complete" && r.targetFront.isTopLevel
            )
          ) {
            this.pageLoadDone();
          }
        },
        ignoreExistingResources: true,
      }
    );
  },

  pageLoadBegin(response) {
    this.resetCollector();
  },

  resetCollector() {
    if (this.collector) {
      this.collector.stop();
    }

    // A page is about to be loaded, start collecting HTTP
    // data from events sent from the backend.
    this.collector = new HarCollector({
      commands: this.commands,
    });

    this.collector.start();
  },

  /**
   * A page is done loading, export collected data. Note that
   * some requests for additional page resources might be pending,
   * so export all after all has been properly received from the backend.
   *
   * This collector still works and collects any consequent HTTP
   * traffic (e.g. XHRs) happening after the page is loaded and
   * The additional traffic can be exported by executing
   * triggerExport on this object.
   */
  pageLoadDone(response) {
    trace.log("HarAutomation.pageLoadDone; ", response);

    if (this.collector) {
      this.collector.waitForHarLoad().then(collector => {
        return this.autoExport();
      });
    }
  },

  autoExport() {
    const autoExport = Services.prefs.getBoolPref(
      prefDomain + "enableAutoExportToFile"
    );

    if (!autoExport) {
      return Promise.resolve();
    }

    // Auto export to file is enabled, so save collected data
    // into a file and use all the default options.
    const data = {
      fileName: Services.prefs.getCharPref(prefDomain + "defaultFileName"),
    };

    return this.executeExport(data);
  },

  // Public API

  /**
   * Export all what is currently collected.
   */
  triggerExport(data) {
    if (!data.fileName) {
      data.fileName = Services.prefs.getCharPref(
        prefDomain + "defaultFileName"
      );
    }

    return this.executeExport(data);
  },

  /**
   * Clear currently collected data.
   */
  clear() {
    this.resetCollector();
  },

  // HAR Export

  /**
   * Execute HAR export. This method fetches all data from the
   * Network panel (asynchronously) and saves it into a file.
   */
  async executeExport(data) {
    const items = this.collector.getItems();
    const { title } = this.commands.targetCommand.targetFront;

    const netMonitor = await this.toolbox.getNetMonitorAPI();
    const connector = await netMonitor.getHarExportConnector();

    const options = {
      connector,
      requestData: null,
      getTimingMarker: null,
      getString: this.getString.bind(this),
      view: this,
      items,
    };

    options.defaultFileName = data.fileName;
    options.compress = data.compress;
    options.title = data.title || title;
    options.id = data.id;
    options.jsonp = data.jsonp;
    options.includeResponseBodies = data.includeResponseBodies;
    options.jsonpCallback = data.jsonpCallback;
    options.forceExport = data.forceExport;

    trace.log("HarAutomation.executeExport; " + data.fileName, options);

    const jsonString = await HarExporter.fetchHarData(options);

    // Save the HAR file if the file name is provided.
    if (jsonString && options.defaultFileName) {
      const file = getDefaultTargetFile(options);
      if (file) {
        HarUtils.saveToFile(file, jsonString, options.compress);
      }
    }

    return jsonString;
  },

  /**
   * Fetches the full text of a string.
   */
  async getString(stringGrip) {
    const fullText = await getLongStringFullText(
      this.commands.client,
      stringGrip
    );
    return fullText;
  },
};

// Protocol Helpers

/**
 * Returns target file for exported HAR data.
 */
function getDefaultTargetFile(options) {
  const path =
    options.defaultLogDir ||
    Services.prefs.getCharPref("devtools.netmonitor.har.defaultLogDir");
  const folder = HarUtils.getLocalDirectory(path);

  const tabTarget = options.connector.getTabTarget();
  const host = new URL(tabTarget.url);
  const fileName = HarUtils.getHarFileName(
    options.defaultFileName,
    options.jsonp,
    options.compress,
    host.hostname
  );

  folder.append(fileName);
  folder.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0666", 8));

  return folder;
}

// Exports from this module
exports.HarAutomation = HarAutomation;
