/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu, Ci, Cc } = require("chrome");
const { Class } = require("sdk/core/heritage");
const { defer, resolve } = require("promise");
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

Cu.import("resource://gre/modules/Task.jsm");

loader.lazyRequireGetter(this, "HarCollector", "devtools/netmonitor/har/har-collector", true);
loader.lazyRequireGetter(this, "HarExporter", "devtools/netmonitor/har/har-exporter", true);
loader.lazyRequireGetter(this, "HarUtils", "devtools/netmonitor/har/har-utils", true);

const prefDomain = "devtools.netmonitor.har.";

// Helper tracer. Should be generic sharable by other modules (bug 1171927)
const trace = {
  log: function(...args) {
  }
}

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
var HarAutomation = Class({
  // Initialization

  initialize: function(toolbox) {
    this.toolbox = toolbox;

    let target = toolbox.target;
    target.makeRemote().then(() => {
      this.startMonitoring(target.client, target.form);
    });
  },

  destroy: function() {
    if (this.collector) {
      this.collector.stop();
    }

    if (this.tabWatcher) {
      this.tabWatcher.disconnect();
    }
  },

  // Automation

  startMonitoring: function(client, tabGrip, callback) {
    if (!client) {
      return;
    }

    if (!tabGrip) {
      return;
    }

    this.debuggerClient = client;
    this.tabClient = this.toolbox.target.activeTab;
    this.webConsoleClient = this.toolbox.target.activeConsole;

    let netPrefs = { "NetworkMonitor.saveRequestAndResponseBodies": true };
    this.webConsoleClient.setPreferences(netPrefs, () => {
      this.tabWatcher = new TabWatcher(this.toolbox, this);
      this.tabWatcher.connect();
    });
  },

  pageLoadBegin: function(aResponse) {
    this.resetCollector();
  },

  resetCollector: function() {
    if (this.collector) {
      this.collector.stop();
    }

    // A page is about to be loaded, start collecting HTTP
    // data from events sent from the backend.
    this.collector = new HarCollector({
      webConsoleClient: this.webConsoleClient,
      debuggerClient: this.debuggerClient
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
  pageLoadDone: function(aResponse) {
    trace.log("HarAutomation.pageLoadDone; ", aResponse);

    if (this.collector) {
      this.collector.waitForHarLoad().then(collector => {
        return this.autoExport();
      });
    }
  },

  autoExport: function() {
    let autoExport = Services.prefs.getBoolPref(prefDomain +
      "enableAutoExportToFile");

    if (!autoExport) {
      return resolve();
    }

    // Auto export to file is enabled, so save collected data
    // into a file and use all the default options.
    let data = {
      fileName: Services.prefs.getCharPref(prefDomain + "defaultFileName"),
    }

    return this.executeExport(data);
  },

  // Public API

  /**
   * Export all what is currently collected.
   */
  triggerExport: function(data) {
    if (!data.fileName) {
      data.fileName = Services.prefs.getCharPref(prefDomain +
        "defaultFileName");
    }

    return this.executeExport(data);
  },

  /**
   * Clear currently collected data.
   */
  clear: function() {
    this.resetCollector();
  },

  // HAR Export

  /**
   * Execute HAR export. This method fetches all data from the
   * Network panel (asynchronously) and saves it into a file.
   */
  executeExport: function(data) {
    let items = this.collector.getItems();
    let form = this.toolbox.target.form;
    let title = form.title || form.url;

    let options = {
      getString: this.getString.bind(this),
      view: this,
      items: items,
    }

    options.defaultFileName = data.fileName;
    options.compress = data.compress;
    options.title = data.title || title;
    options.id = data.id;
    options.jsonp = data.jsonp;
    options.includeResponseBodies = data.includeResponseBodies;
    options.jsonpCallback = data.jsonpCallback;
    options.forceExport = data.forceExport;

    trace.log("HarAutomation.executeExport; " + data.fileName, options);

    return HarExporter.fetchHarData(options).then(jsonString => {
      // Save the HAR file if the file name is provided.
      if (jsonString && options.defaultFileName) {
        let file = getDefaultTargetFile(options);
        if (file) {
          HarUtils.saveToFile(file, jsonString, options.compress);
        }
      }

      return jsonString;
    });
  },

  /**
   * Fetches the full text of a string.
   */
  getString: function(aStringGrip) {
    return this.webConsoleClient.getString(aStringGrip);
  },

  /**
   * Extracts any urlencoded form data sections (e.g. "?foo=bar&baz=42") from a
   * POST request.
   *
   * @param object aHeaders
   *        The "requestHeaders".
   * @param object aUploadHeaders
   *        The "requestHeadersFromUploadStream".
   * @param object aPostData
   *        The "requestPostData".
   * @return array
   *        A promise that is resolved with the extracted form data.
   */
  _getFormDataSections: Task.async(function*(aHeaders, aUploadHeaders, aPostData) {
    let formDataSections = [];

    let { headers: requestHeaders } = aHeaders;
    let { headers: payloadHeaders } = aUploadHeaders;
    let allHeaders = [...payloadHeaders, ...requestHeaders];

    let contentTypeHeader = allHeaders.find(e => e.name.toLowerCase() == "content-type");
    let contentTypeLongString = contentTypeHeader ? contentTypeHeader.value : "";
    let contentType = yield this.getString(contentTypeLongString);

    if (contentType.includes("x-www-form-urlencoded")) {
      let postDataLongString = aPostData.postData.text;
      let postData = yield this.getString(postDataLongString);

      for (let section of postData.split(/\r\n|\r|\n/)) {
        // Before displaying it, make sure this section of the POST data
        // isn't a line containing upload stream headers.
        if (payloadHeaders.every(header => !section.startsWith(header.name))) {
          formDataSections.push(section);
        }
      }
    }

    return formDataSections;
  }),
});

// Helpers

function TabWatcher(toolbox, listener) {
  this.target = toolbox.target;
  this.listener = listener;

  this.onTabNavigated = this.onTabNavigated.bind(this);
}

TabWatcher.prototype = {
  // Connection

  connect: function() {
    this.target.on("navigate", this.onTabNavigated);
    this.target.on("will-navigate", this.onTabNavigated);
  },

  disconnect: function() {
    if (!this.target) {
      return;
    }

    this.target.off("navigate", this.onTabNavigated);
    this.target.off("will-navigate", this.onTabNavigated);
  },

  // Event Handlers

  /**
   * Called for each location change in the monitored tab.
   *
   * @param string aType
   *        Packet type.
   * @param object aPacket
   *        Packet received from the server.
   */
  onTabNavigated: function(aType, aPacket) {
    switch (aType) {
      case "will-navigate": {
        this.listener.pageLoadBegin(aPacket);
        break;
      }
      case "navigate": {
        this.listener.pageLoadDone(aPacket);
        break;
      }
    }
  },
};

// Protocol Helpers

/**
 * Returns target file for exported HAR data.
 */
function getDefaultTargetFile(options) {
  let path = options.defaultLogDir ||
    Services.prefs.getCharPref("devtools.netmonitor.har.defaultLogDir");
  let folder = HarUtils.getLocalDirectory(path);
  let fileName = HarUtils.getHarFileName(options.defaultFileName,
    options.jsonp, options.compress);

  folder.append(fileName);
  folder.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0666", 8));

  return folder;
}

// Exports from this module
exports.HarAutomation = HarAutomation;
