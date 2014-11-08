/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Telemetry.
 *
 * To add metrics for a tool:
 *
 * 1. Create boolean, flag and exponential entries in
 *    toolkit/components/telemetry/Histograms.json. Each type is optional but it
 *    is best if all three can be included.
 *
 * 2. Add your chart entries to browser/devtools/shared/telemetry.js
 *    (Telemetry.prototype._histograms):
 *    mytoolname: {
 *      histogram: "DEVTOOLS_MYTOOLNAME_OPENED_BOOLEAN",
 *      userHistogram: "DEVTOOLS_MYTOOLNAME_OPENED_PER_USER_FLAG",
 *      timerHistogram: "DEVTOOLS_MYTOOLNAME_TIME_ACTIVE_SECONDS"
 *    },
 *
 * 3. Include this module at the top of your tool. Use:
 *      let Telemetry = require("devtools/shared/telemetry")
 *
 * 4. Create a telemetry instance in your tool's constructor:
 *      this._telemetry = new Telemetry();
 *
 * 5. When your tool is opened call:
 *      this._telemetry.toolOpened("mytoolname");
 *
 * 6. When your tool is closed call:
 *      this._telemetry.toolClosed("mytoolname");
 *
 * Note:
 * You can view telemetry stats for your local Firefox instance via
 * about:telemetry.
 *
 * You can view telemetry stats for large groups of Firefox users at
 * telemetry.mozilla.org.
 */

const TOOLS_OPENED_PREF = "devtools.telemetry.tools.opened.version";

this.Telemetry = function() {
  // Bind pretty much all functions so that callers do not need to.
  this.toolOpened = this.toolOpened.bind(this);
  this.toolClosed = this.toolClosed.bind(this);
  this.log = this.log.bind(this);
  this.logOncePerBrowserVersion = this.logOncePerBrowserVersion.bind(this);
  this.destroy = this.destroy.bind(this);

  this._timers = new Map();
};

module.exports = Telemetry;

let {Cc, Ci, Cu} = require("chrome");
let {Services} = Cu.import("resource://gre/modules/Services.jsm", {});
let {XPCOMUtils} = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});

Telemetry.prototype = {
  _histograms: {
    toolbox: {
      histogram: "DEVTOOLS_TOOLBOX_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_TOOLBOX_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_TOOLBOX_TIME_ACTIVE_SECONDS"
    },
    options: {
      histogram: "DEVTOOLS_OPTIONS_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_OPTIONS_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_OPTIONS_TIME_ACTIVE_SECONDS"
    },
    webconsole: {
      histogram: "DEVTOOLS_WEBCONSOLE_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_WEBCONSOLE_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_WEBCONSOLE_TIME_ACTIVE_SECONDS"
    },
    browserconsole: {
      histogram: "DEVTOOLS_BROWSERCONSOLE_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_BROWSERCONSOLE_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_BROWSERCONSOLE_TIME_ACTIVE_SECONDS"
    },
    inspector: {
      histogram: "DEVTOOLS_INSPECTOR_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_INSPECTOR_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_INSPECTOR_TIME_ACTIVE_SECONDS"
    },
    ruleview: {
      histogram: "DEVTOOLS_RULEVIEW_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_RULEVIEW_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_RULEVIEW_TIME_ACTIVE_SECONDS"
    },
    computedview: {
      histogram: "DEVTOOLS_COMPUTEDVIEW_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_COMPUTEDVIEW_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_COMPUTEDVIEW_TIME_ACTIVE_SECONDS"
    },
    layoutview: {
      histogram: "DEVTOOLS_LAYOUTVIEW_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_LAYOUTVIEW_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_LAYOUTVIEW_TIME_ACTIVE_SECONDS"
    },
    fontinspector: {
      histogram: "DEVTOOLS_FONTINSPECTOR_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_FONTINSPECTOR_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_FONTINSPECTOR_TIME_ACTIVE_SECONDS"
    },
    jsdebugger: {
      histogram: "DEVTOOLS_JSDEBUGGER_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_JSDEBUGGER_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_JSDEBUGGER_TIME_ACTIVE_SECONDS"
    },
    jsbrowserdebugger: {
      histogram: "DEVTOOLS_JSBROWSERDEBUGGER_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_JSBROWSERDEBUGGER_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_JSBROWSERDEBUGGER_TIME_ACTIVE_SECONDS"
    },
    styleeditor: {
      histogram: "DEVTOOLS_STYLEEDITOR_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_STYLEEDITOR_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_STYLEEDITOR_TIME_ACTIVE_SECONDS"
    },
    shadereditor: {
      histogram: "DEVTOOLS_SHADEREDITOR_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_SHADEREDITOR_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_SHADEREDITOR_TIME_ACTIVE_SECONDS"
    },
    webaudioeditor: {
      histogram: "DEVTOOLS_WEBAUDIOEDITOR_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_WEBAUDIOEDITOR_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_WEBAUDIOEDITOR_TIME_ACTIVE_SECONDS"
    },
    canvasdebugger: {
      histogram: "DEVTOOLS_CANVASDEBUGGER_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_CANVASDEBUGGER_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_CANVASDEBUGGER_TIME_ACTIVE_SECONDS"
    },
    jsprofiler: {
      histogram: "DEVTOOLS_JSPROFILER_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_JSPROFILER_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_JSPROFILER_TIME_ACTIVE_SECONDS"
    },
    netmonitor: {
      histogram: "DEVTOOLS_NETMONITOR_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_NETMONITOR_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_NETMONITOR_TIME_ACTIVE_SECONDS"
    },
    tilt: {
      histogram: "DEVTOOLS_TILT_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_TILT_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_TILT_TIME_ACTIVE_SECONDS"
    },
    paintflashing: {
      histogram: "DEVTOOLS_PAINTFLASHING_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_PAINTFLASHING_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_PAINTFLASHING_TIME_ACTIVE_SECONDS"
    },
    scratchpad: {
      histogram: "DEVTOOLS_SCRATCHPAD_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_SCRATCHPAD_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_SCRATCHPAD_TIME_ACTIVE_SECONDS"
    },
    responsive: {
      histogram: "DEVTOOLS_RESPONSIVE_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_RESPONSIVE_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_RESPONSIVE_TIME_ACTIVE_SECONDS"
    },
    developertoolbar: {
      histogram: "DEVTOOLS_DEVELOPERTOOLBAR_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_DEVELOPERTOOLBAR_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_DEVELOPERTOOLBAR_TIME_ACTIVE_SECONDS"
    },
    webide: {
      histogram: "DEVTOOLS_WEBIDE_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_WEBIDE_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_WEBIDE_TIME_ACTIVE_SECONDS"
    },
    custom: {
      histogram: "DEVTOOLS_CUSTOM_OPENED_BOOLEAN",
      userHistogram: "DEVTOOLS_CUSTOM_OPENED_PER_USER_FLAG",
      timerHistogram: "DEVTOOLS_CUSTOM_TIME_ACTIVE_SECONDS"
    }
  },

  /**
   * Add an entry to a histogram.
   *
   * @param  {String} id
   *         Used to look up the relevant histogram ID and log true to that
   *         histogram.
   */
  toolOpened: function(id) {
    let charts = this._histograms[id] || this._histograms.custom;

    if (charts.histogram) {
      this.log(charts.histogram, true);
    }
    if (charts.userHistogram) {
      this.logOncePerBrowserVersion(charts.userHistogram, true);
    }
    if (charts.timerHistogram) {
      this.startTimer(charts.timerHistogram);
    }
  },

  toolClosed: function(id) {
    let charts = this._histograms[id];

    if (!charts || !charts.timerHistogram) {
      return;
    }

    this.stopTimer(charts.timerHistogram);
  },

  /**
   * Record the start time for a timing-based histogram entry.
   *
   * @param String histogramId
   *        Histogram in which the data is to be stored.
   */
  startTimer: function(histogramId) {
    this._timers.set(histogramId, new Date());
  },

  /**
   * Stop the timer and log elasped time for a timing-based histogram entry.
   *
   * @param String histogramId
   *        Histogram in which the data is to be stored.
   */
  stopTimer: function(histogramId) {
    let startTime = this._timers.get(histogramId);
    if (startTime) {
      let time = (new Date() - startTime) / 1000;
      this.log(histogramId, time);
      this._timers.delete(histogramId);
    }
  },

  /**
   * Log a value to a histogram.
   *
   * @param  {String} histogramId
   *         Histogram in which the data is to be stored.
   * @param  value
   *         Value to store.
   */
  log: function(histogramId, value) {
    if (histogramId) {
      try {
        let histogram = Services.telemetry.getHistogramById(histogramId);
        histogram.add(value);
      } catch(e) {
        dump("Warning: An attempt was made to write to the " + histogramId +
             " histogram, which is not defined in Histograms.json\n");
      }
    }
  },

  /**
   * Log info about usage once per browser version. This allows us to discover
   * how many individual users are using our tools for each browser version.
   *
   * @param  {String} perUserHistogram
   *         Histogram in which the data is to be stored.
   */
  logOncePerBrowserVersion: function(perUserHistogram, value) {
    let currentVersion = appInfo.version;
    let latest = Services.prefs.getCharPref(TOOLS_OPENED_PREF);
    let latestObj = JSON.parse(latest);

    let lastVersionHistogramUpdated = latestObj[perUserHistogram];

    if (typeof lastVersionHistogramUpdated == "undefined" ||
        lastVersionHistogramUpdated !== currentVersion) {
      latestObj[perUserHistogram] = currentVersion;
      latest = JSON.stringify(latestObj);
      Services.prefs.setCharPref(TOOLS_OPENED_PREF, latest);
      this.log(perUserHistogram, value);
    }
  },

  destroy: function() {
    for (let histogramId of this._timers.keys()) {
      this.stopTimer(histogramId);
    }
  }
};

XPCOMUtils.defineLazyGetter(this, "appInfo", function() {
  return Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo);
});
