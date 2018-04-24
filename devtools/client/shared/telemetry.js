/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is the telemetry module to report metrics for tools.
 *
 * Comprehensive documentation is in docs/frontend/telemetry.md
 */

"use strict";

const Services = require("Services");
const TOOLS_OPENED_PREF = "devtools.telemetry.tools.opened.version";

// Object to be shared among all instances.
const PENDING_EVENTS = new Map();
const PENDING_EVENT_PROPERTIES = new Map();

class Telemetry {
  constructor() {
    // Bind pretty much all functions so that callers do not need to.
    this.toolOpened = this.toolOpened.bind(this);
    this.toolClosed = this.toolClosed.bind(this);
    this.log = this.log.bind(this);
    this.logScalar = this.logScalar.bind(this);
    this.logCountScalar = this.logCountScalar.bind(this);
    this.logKeyedScalar = this.logKeyedScalar.bind(this);
    this.logOncePerBrowserVersion = this.logOncePerBrowserVersion.bind(this);
    this.recordEvent = this.recordEvent.bind(this);
    this.setEventRecordingEnabled = this.setEventRecordingEnabled.bind(this);
    this.preparePendingEvent = this.preparePendingEvent.bind(this);
    this.addEventProperty = this.addEventProperty.bind(this);
    this.destroy = this.destroy.bind(this);

    this._timers = new Map();
  }

  get histograms() {
    return {
      toolbox: {
        histogram: "DEVTOOLS_TOOLBOX_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_TOOLBOX_TIME_ACTIVE_SECONDS"
      },
      options: {
        histogram: "DEVTOOLS_OPTIONS_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_OPTIONS_TIME_ACTIVE_SECONDS"
      },
      webconsole: {
        histogram: "DEVTOOLS_WEBCONSOLE_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_WEBCONSOLE_TIME_ACTIVE_SECONDS"
      },
      browserconsole: {
        histogram: "DEVTOOLS_BROWSERCONSOLE_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_BROWSERCONSOLE_TIME_ACTIVE_SECONDS"
      },
      inspector: {
        histogram: "DEVTOOLS_INSPECTOR_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_INSPECTOR_TIME_ACTIVE_SECONDS"
      },
      ruleview: {
        histogram: "DEVTOOLS_RULEVIEW_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_RULEVIEW_TIME_ACTIVE_SECONDS"
      },
      computedview: {
        histogram: "DEVTOOLS_COMPUTEDVIEW_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_COMPUTEDVIEW_TIME_ACTIVE_SECONDS"
      },
      layoutview: {
        histogram: "DEVTOOLS_LAYOUTVIEW_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_LAYOUTVIEW_TIME_ACTIVE_SECONDS"
      },
      fontinspector: {
        histogram: "DEVTOOLS_FONTINSPECTOR_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_FONTINSPECTOR_TIME_ACTIVE_SECONDS"
      },
      animationinspector: {
        histogram: "DEVTOOLS_ANIMATIONINSPECTOR_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_ANIMATIONINSPECTOR_TIME_ACTIVE_SECONDS"
      },
      jsdebugger: {
        histogram: "DEVTOOLS_JSDEBUGGER_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_JSDEBUGGER_TIME_ACTIVE_SECONDS"
      },
      jsbrowserdebugger: {
        histogram: "DEVTOOLS_JSBROWSERDEBUGGER_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_JSBROWSERDEBUGGER_TIME_ACTIVE_SECONDS"
      },
      styleeditor: {
        histogram: "DEVTOOLS_STYLEEDITOR_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_STYLEEDITOR_TIME_ACTIVE_SECONDS"
      },
      shadereditor: {
        histogram: "DEVTOOLS_SHADEREDITOR_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_SHADEREDITOR_TIME_ACTIVE_SECONDS"
      },
      webaudioeditor: {
        histogram: "DEVTOOLS_WEBAUDIOEDITOR_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_WEBAUDIOEDITOR_TIME_ACTIVE_SECONDS"
      },
      canvasdebugger: {
        histogram: "DEVTOOLS_CANVASDEBUGGER_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_CANVASDEBUGGER_TIME_ACTIVE_SECONDS"
      },
      performance: {
        histogram: "DEVTOOLS_JSPROFILER_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_JSPROFILER_TIME_ACTIVE_SECONDS"
      },
      memory: {
        histogram: "DEVTOOLS_MEMORY_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_MEMORY_TIME_ACTIVE_SECONDS"
      },
      netmonitor: {
        histogram: "DEVTOOLS_NETMONITOR_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_NETMONITOR_TIME_ACTIVE_SECONDS"
      },
      storage: {
        histogram: "DEVTOOLS_STORAGE_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_STORAGE_TIME_ACTIVE_SECONDS"
      },
      dom: {
        histogram: "DEVTOOLS_DOM_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_DOM_TIME_ACTIVE_SECONDS"
      },
      paintflashing: {
        histogram: "DEVTOOLS_PAINTFLASHING_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_PAINTFLASHING_TIME_ACTIVE_SECONDS"
      },
      scratchpad: {
        histogram: "DEVTOOLS_SCRATCHPAD_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_SCRATCHPAD_TIME_ACTIVE_SECONDS"
      },
      "scratchpad-window": {
        histogram: "DEVTOOLS_SCRATCHPAD_WINDOW_OPENED_COUNT",
      },
      responsive: {
        histogram: "DEVTOOLS_RESPONSIVE_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_RESPONSIVE_TIME_ACTIVE_SECONDS"
      },
      eyedropper: {
        histogram: "DEVTOOLS_EYEDROPPER_OPENED_COUNT",
      },
      menueyedropper: {
        histogram: "DEVTOOLS_MENU_EYEDROPPER_OPENED_COUNT",
      },
      pickereyedropper: {
        histogram: "DEVTOOLS_PICKER_EYEDROPPER_OPENED_COUNT",
      },
      toolbareyedropper: {
        scalar: "devtools.toolbar.eyedropper.opened",
      },
      copyuniquecssselector: {
        scalar: "devtools.copy.unique.css.selector.opened",
      },
      copyfullcssselector: {
        scalar: "devtools.copy.full.css.selector.opened",
      },
      copyxpath: {
        scalar: "devtools.copy.xpath.opened",
      },
      developertoolbar: {
        histogram: "DEVTOOLS_DEVELOPERTOOLBAR_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_DEVELOPERTOOLBAR_TIME_ACTIVE_SECONDS"
      },
      aboutdebugging: {
        histogram: "DEVTOOLS_ABOUTDEBUGGING_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_ABOUTDEBUGGING_TIME_ACTIVE_SECONDS"
      },
      webide: {
        histogram: "DEVTOOLS_WEBIDE_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_WEBIDE_TIME_ACTIVE_SECONDS"
      },
      webideNewProject: {
        histogram: "DEVTOOLS_WEBIDE_NEW_PROJECT_COUNT",
      },
      webideImportProject: {
        histogram: "DEVTOOLS_WEBIDE_IMPORT_PROJECT_COUNT",
      },
      custom: {
        histogram: "DEVTOOLS_CUSTOM_OPENED_COUNT",
        timerHistogram: "DEVTOOLS_CUSTOM_TIME_ACTIVE_SECONDS"
      },
      gridInspectorShowGridAreasOverlayChecked: {
        scalar: "devtools.grid.showGridAreasOverlay.checked",
      },
      gridInspectorShowGridLineNumbersChecked: {
        scalar: "devtools.grid.showGridLineNumbers.checked",
      },
      gridInspectorShowInfiniteLinesChecked: {
        scalar: "devtools.grid.showInfiniteLines.checked",
      },
      accessibility: {
        countScalar: "devtools.accessibility.opened_count",
        timerHistogram: "DEVTOOLS_ACCESSIBILITY_TIME_ACTIVE_SECONDS"
      },
      accessibilityNodeInspected: {
        countScalar: "devtools.accessibility.node_inspected_count"
      },
      accessibilityPickerUsed: {
        countScalar: "devtools.accessibility.picker_used_count",
        timerHistogram: "DEVTOOLS_ACCESSIBILITY_PICKER_TIME_ACTIVE_SECONDS"
      }
    };
  }

  /**
   * Add an entry to a histogram.
   *
   * @param  {String} id
   *         Used to look up the relevant histogram ID and log true to that
   *         histogram.
   */
  toolOpened(id) {
    let charts = this.histograms[id] || this.histograms.custom;

    if (charts.histogram) {
      this.log(charts.histogram, true);
    }
    if (charts.timerHistogram) {
      this.startTimer(charts.timerHistogram);
    }
    if (charts.scalar) {
      this.logScalar(charts.scalar, 1);
    }
    if (charts.countScalar) {
      this.logCountScalar(charts.countScalar, 1);
    }
  }

  /**
   * Record that an action occurred.  Aliases to `toolOpened`, so it's just for
   * readability at the call site for cases where we aren't actually opening
   * tools.
   */
  actionOccurred(id) {
    this.toolOpened(id);
  }

  toolClosed(id) {
    let charts = this.histograms[id];

    if (!charts || !charts.timerHistogram) {
      return;
    }

    this.stopTimer(charts.timerHistogram);
  }

  /**
   * Record the start time for a timing-based histogram entry.
   *
   * @param String histogramId
   *        Histogram in which the data is to be stored.
   */
  startTimer(histogramId) {
    this._timers.set(histogramId, new Date());
  }

  /**
   * Stop the timer and log elasped time for a timing-based histogram entry.
   *
   * @param String histogramId
   *        Histogram in which the data is to be stored.
   * @param String key [optional]
   *        Optional key for a keyed histogram.
   */
  stopTimer(histogramId, key) {
    let startTime = this._timers.get(histogramId);
    if (startTime) {
      let time = (new Date() - startTime) / 1000;
      if (!key) {
        this.log(histogramId, time);
      } else {
        this.logKeyed(histogramId, key, time);
      }
      this._timers.delete(histogramId);
    }
  }

  /**
   * Log a value to a histogram.
   *
   * @param  {String} histogramId
   *         Histogram in which the data is to be stored.
   * @param  value
   *         Value to store.
   */
  log(histogramId, value) {
    if (!histogramId) {
      return;
    }

    try {
      let histogram = Services.telemetry.getHistogramById(histogramId);
      histogram.add(value);
    } catch (e) {
      dump(`Warning: An attempt was made to write to the ${histogramId} ` +
           `histogram, which is not defined in Histograms.json\n`);
    }
  }

  /**
   * Log a value to a scalar.
   *
   * @param  {String} scalarId
   *         Scalar in which the data is to be stored.
   * @param  value
   *         Value to store.
   */
  logScalar(scalarId, value) {
    if (!scalarId) {
      return;
    }

    try {
      if (isNaN(value) && typeof value !== "boolean") {
        dump(`Warning: An attempt was made to write a non-numeric and ` +
             `non-boolean value ${value} to the ${scalarId} scalar. Only ` +
             `numeric and boolean values are allowed.\n`);

        return;
      }
      Services.telemetry.scalarSet(scalarId, value);
    } catch (e) {
      dump(`Warning: An attempt was made to write to the ${scalarId} ` +
           `scalar, which is not defined in Scalars.yaml\n`);
    }
  }

  /**
   * Log a value to a count scalar.
   *
   * @param  {String} scalarId
   *         Scalar in which the data is to be stored.
   * @param  value
   *         Value to store.
   */
  logCountScalar(scalarId, value) {
    if (!scalarId) {
      return;
    }

    try {
      if (isNaN(value)) {
        dump(`Warning: An attempt was made to write a non-numeric value ` +
             `${value} to the ${scalarId} scalar. Only numeric values are ` +
             `allowed.\n`);

        return;
      }
      Services.telemetry.scalarAdd(scalarId, value);
    } catch (e) {
      dump(`Warning: An attempt was made to write to the ${scalarId} ` +
           `scalar, which is not defined in Scalars.yaml\n`);
    }
  }

  /**
   * Log a value to a keyed count scalar.
   *
   * @param  {String} scalarId
   *         Scalar in which the data is to be stored.
   * @param  {String} key
   *         The key within the  scalar.
   * @param  value
   *         Value to store.
   */
  logKeyedScalar(scalarId, key, value) {
    if (!scalarId) {
      return;
    }

    try {
      if (isNaN(value)) {
        dump(`Warning: An attempt was made to write a non-numeric value ` +
             `${value} to the ${scalarId} scalar. Only numeric values are ` +
             `allowed.\n`);

        return;
      }
      Services.telemetry.keyedScalarAdd(scalarId, key, value);
    } catch (e) {
      dump(`Warning: An attempt was made to write to the ${scalarId} ` +
           `scalar, which is not defined in Scalars.yaml\n`);
    }
  }

  /**
   * Log a value to a keyed histogram.
   *
   * @param  {String} histogramId
   *         Histogram in which the data is to be stored.
   * @param  {String} key
   *         The key within the single histogram.
   * @param  [value]
   *         Optional value to store.
   */
  logKeyed(histogramId, key, value) {
    if (histogramId) {
      try {
        let histogram = Services.telemetry.getKeyedHistogramById(histogramId);

        if (typeof value === "undefined") {
          histogram.add(key);
        } else {
          histogram.add(key, value);
        }
      } catch (e) {
        dump(`Warning: An attempt was made to write to the ${histogramId} ` +
             `histogram, which is not defined in Histograms.json\n`);
      }
    }
  }

  /**
   * Log info about usage once per browser version. This allows us to discover
   * how many individual users are using our tools for each browser version.
   *
   * @param  {String} perUserHistogram
   *         Histogram in which the data is to be stored.
   */
  logOncePerBrowserVersion(perUserHistogram, value) {
    let currentVersion = Services.appinfo.version;
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
  }

  /**
   * Event telemetry is disabled by default. Use this method to enable it for
   * a particular category.
   *
   * @param {String} category
   *        The telemetry event category e.g. "devtools.main"
   * @param {Boolean} enabled
   *        Enabled: true or false.
   */
  setEventRecordingEnabled(category, enabled) {
    return Services.telemetry.setEventRecordingEnabled(category, enabled);
  }

  /**
   * Telemetry events often need to make use of a number of properties from
   * completely different codepaths. To make this possible we create a
   * "pending event" along with an array of property names that we need to wait
   * for before sending the event.
   *
   * As each property is received via addEventProperty() we check if all
   * properties have been received. Once they have all been received we send the
   * telemetry event.
   *
   * @param {String} category
   *        The telemetry event category (a group name for events and helps to
   *        avoid name conflicts) e.g. "devtools.main"
   * @param {String} method
   *        The telemetry event method (describes the type of event that
   *        occurred e.g. "open")
   * @param {String} object
   *        The telemetry event object name (the name of the object the event
   *        occurred on) e.g. "tools" or "setting"
   * @param {String|null} value
   *        The telemetry event value (a user defined value, providing context
   *        for the event) e.g. "console"
   * @param {Array} expected
   *        An array of the properties needed before sending the telemetry
   *        event e.g.
   *        [
   *          "host",
   *          "width"
   *        ]
   */
  preparePendingEvent(category, method, object, value, expected = []) {
    const sig = `${category},${method},${object},${value}`;

    if (expected.length === 0) {
      throw new Error(`preparePendingEvent() was called without any expected ` +
                      `properties.`);
    }

    PENDING_EVENTS.set(sig, {
      extra: {},
      expected: new Set(expected)
    });

    const props = PENDING_EVENT_PROPERTIES.get(sig);
    if (props) {
      for (let [name, val] of Object.entries(props)) {
        this.addEventProperty(category, method, object, value, name, val);
      }
      PENDING_EVENT_PROPERTIES.delete(sig);
    }
  }

  /**
   * Adds an expected property for either a current or future pending event.
   * This means that if preparePendingEvent() is called before or after sending
   * the event properties they will automatically added to the event.
   *
   * @param {String} category
   *        The telemetry event category (a group name for events and helps to
   *        avoid name conflicts) e.g. "devtools.main"
   * @param {String} method
   *        The telemetry event method (describes the type of event that
   *        occurred e.g. "open")
   * @param {String} object
   *        The telemetry event object name (the name of the object the event
   *        occurred on) e.g. "tools" or "setting"
   * @param {String|null} value
   *        The telemetry event value (a user defined value, providing context
   *        for the event) e.g. "console"
   * @param {String} pendingPropName
   *        The pending property name
   * @param {String} pendingPropValue
   *        The pending property value
   */
  addEventProperty(category, method, object, value, pendingPropName, pendingPropValue) {
    const sig = `${category},${method},${object},${value}`;

    // If the pending event has not been created add the property to the pending
    // list.
    if (!PENDING_EVENTS.has(sig)) {
      let props = PENDING_EVENT_PROPERTIES.get(sig);

      if (props) {
        props[pendingPropName] = pendingPropValue;
      } else {
        PENDING_EVENT_PROPERTIES.set(sig, {
          [pendingPropName]: pendingPropValue
        });
      }
      return;
    }

    const { expected, extra } = PENDING_EVENTS.get(sig);

    if (expected.has(pendingPropName)) {
      extra[pendingPropName] = pendingPropValue;

      if (expected.size === Object.keys(extra).length) {
        this._sendPendingEvent(category, method, object, value);
      }
    } else {
      // The property was not expected, warn and bail.
      throw new Error(`An attempt was made to add the unexpected property ` +
                      `"${pendingPropName}" to a telemetry event with the ` +
                      `signature "${sig}"\n`);
    }
  }

  /**
   * Adds expected properties for either a current or future pending event.
   * This means that if preparePendingEvent() is called before or after sending
   * the event properties they will automatically added to the event.
   *
   * @param {String} category
   *        The telemetry event category (a group name for events and helps to
   *        avoid name conflicts) e.g. "devtools.main"
   * @param {String} method
   *        The telemetry event method (describes the type of event that
   *        occurred e.g. "open")
   * @param {String} object
   *        The telemetry event object name (the name of the object the event
   *        occurred on) e.g. "tools" or "setting"
   * @param {String|null} value
   *        The telemetry event value (a user defined value, providing context
   *        for the event) e.g. "console"
   * @param {String} pendingObject
   *        An object containing key, value pairs that should be added to the
   *        event as properties.
   */
  addEventProperties(category, method, object, value, pendingObject) {
    for (let [key, val] of Object.entries(pendingObject)) {
      this.addEventProperty(category, method, object, value, key, val);
    }
  }

  /**
   * Send a telemetry event.
   *
   * @param {String} category
   *        The telemetry event category (a group name for events and helps to
   *        avoid name conflicts) e.g. "devtools.main"
   * @param {String} method
   *        The telemetry event method (describes the type of event that
   *        occurred e.g. "open")
   * @param {String} object
   *        The telemetry event object name (the name of the object the event
   *        occurred on) e.g. "tools" or "setting"
   * @param {String|null} value
   *        The telemetry event value (a user defined value, providing context
   *        for the event) e.g. "console"
   * @param {Object} extra
   *        The telemetry event extra object containing the properties that will
   *        be sent with the event e.g.
   *        {
   *          host: "bottom",
   *          width: "1024"
   *        }
   */
  recordEvent(category, method, object, value, extra) {
    // Only string values are allowed so cast all values to strings.
    for (let [name, val] of Object.entries(extra)) {
      val = val + "";
      extra[name] = val;

      if (val.length > 80) {
        const sig = `${category},${method},${object},${value}`;

        throw new Error(`The property "${name}" was added to a telemetry ` +
                        `event with the signature ${sig} but it's value ` +
                        `"${val}" is longer than the maximum allowed length ` +
                        `of 80 characters\n`);
      }
    }
    Services.telemetry.recordEvent(category, method, object, value, extra);
  }

  /**
   * A private method that is not to be used externally. This method is used to
   * prepare a pending telemetry event for sending and then send it via
   * recordEvent().
   *
   * @param {String} category
   *        The telemetry event category (a group name for events and helps to
   *        avoid name conflicts) e.g. "devtools.main"
   * @param {String} method
   *        The telemetry event method (describes the type of event that
   *        occurred e.g. "open")
   * @param {String} object
   *        The telemetry event object name (the name of the object the event
   *        occurred on) e.g. "tools" or "setting"
   * @param {String|null} value
   *        The telemetry event value (a user defined value, providing context
   *        for the event) e.g. "console"
   */
  _sendPendingEvent(category, method, object, value) {
    const sig = `${category},${method},${object},${value}`;
    const { extra } = PENDING_EVENTS.get(sig);

    PENDING_EVENTS.delete(sig);
    PENDING_EVENT_PROPERTIES.delete(sig);
    this.recordEvent(category, method, object, value, extra);
  }

  destroy() {
    for (let histogramId of this._timers.keys()) {
      this.stopTimer(histogramId);
    }
  }
}

module.exports = Telemetry;
