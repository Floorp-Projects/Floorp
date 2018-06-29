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
const { TelemetryStopwatch } = require("resource://gre/modules/TelemetryStopwatch.jsm");
const { getNthPathExcluding } = require("devtools/shared/platform/stack");

// Object to be shared among all instances.
const PENDING_EVENTS = new Map();
const PENDING_EVENT_PROPERTIES = new Map();

class Telemetry {
  constructor() {
    // Bind pretty much all functions so that callers do not need to.
    this.msSystemNow = this.msSystemNow.bind(this);
    this.getHistogramById = this.getHistogramById.bind(this);
    this.getKeyedHistogramById = this.getKeyedHistogramById.bind(this);
    this.scalarSet = this.scalarSet.bind(this);
    this.scalarAdd = this.scalarAdd.bind(this);
    this.keyedScalarAdd = this.keyedScalarAdd.bind(this);
    this.keyedScalarSet = this.keyedScalarSet.bind(this);
    this.recordEvent = this.recordEvent.bind(this);
    this.setEventRecordingEnabled = this.setEventRecordingEnabled.bind(this);
    this.preparePendingEvent = this.preparePendingEvent.bind(this);
    this.addEventProperty = this.addEventProperty.bind(this);
    this.toolOpened = this.toolOpened.bind(this);
    this.toolClosed = this.toolClosed.bind(this);
  }

  /**
   * Time since the system wide epoch. This is not a monotonic timer but
   * can be used across process boundaries.
   */
  msSystemNow() {
    return Services.telemetry.msSystemNow();
  }

  /**
   * Starts a timer associated with a telemetry histogram. The timer can be
   * directly associated with a histogram, or with a pair of a histogram and
   * an object.
   *
   * @param {String} histogramId
   *        A string which must be a valid histogram name.
   * @param {Object} obj
   *        Optional parameter. If specified, the timer is associated with this
   *        object, meaning that multiple timers for the same histogram may be
   *        run concurrently, as long as they are associated with different
   *        objects.
   *
   * @returns {Boolean}
   *          True if the timer was successfully started, false otherwise. If a
   *          timer already exists, it can't be started again, and the existing
   *          one will be cleared in order to avoid measurements errors.
   */
  start(histogramId, obj) {
    return TelemetryStopwatch.start(histogramId, obj);
  }

  /**
   * Starts a timer associated with a keyed telemetry histogram. The timer can
   * be directly associated with a histogram and its key. Similarly to
   * TelemetryStopwatch.start the histogram and its key can be associated
   * with an object. Each key may have multiple associated objects and each
   * object can be associated with multiple keys.
   *
   * @param {String} histogramId
   *        A string which must be a valid histogram name.
   * @param {String} key
   *        A string which must be a valid histgram key.
   * @param {Object} obj
   *        Optional parameter. If specified, the timer is associated with this
   *        object, meaning that multiple timers for the same histogram may be
   *        run concurrently,as long as they are associated with different
   *        objects.
   *
   * @returns {Boolean}
   *          True if the timer was successfully started, false otherwise. If a
   *          timer already exists, it can't be started again, and the existing
   *          one will be cleared in order to avoid measurements errors.
   */
  startKeyed(histogramId, key, obj) {
    return TelemetryStopwatch.startKeyed(histogramId, key, obj);
  }

  /**
   * Stops the timer associated with the given histogram (and object),
   * calculates the time delta between start and finish, and adds the value
   * to the histogram.
   *
   * @param {String} histogramId
   *        A string which must be a valid histogram name.
   * @param {Object} obj
   *        Optional parameter which associates the histogram timer with the
   *        given object.
   * @param {Boolean} canceledOkay
   *        Optional parameter which will suppress any warnings that normally
   *        fire when a stopwatch is finished after being canceled.
   *        Defaults to false.
   *
   * @returns {Boolean}
   *          True if the timer was succesfully stopped and the data was added
   *          to the histogram, False otherwise.
   */
  finish(histogramId, obj, canceledOkay) {
    return TelemetryStopwatch.finish(histogramId, obj, canceledOkay);
  }

  /**
   * Stops the timer associated with the given keyed histogram (and object),
   * calculates the time delta between start and finish, and adds the value
   * to the keyed histogram.
   *
   * @param {String} histogramId
   *        A string which must be a valid histogram name.
   * @param {String} key
   *        A string which must be a valid histogram key.
   * @param {Object} obj
   *        Optional parameter which associates the histogram timer with the
   *        given object.
   * @param {Boolean} canceledOkay
   *        Optional parameter which will suppress any warnings that normally
   *        fire when a stopwatch is finished after being canceled.
   *        Defaults to false.
   *
   * @returns {Boolean}
   *          True if the timer was succesfully stopped and the data was added
   *          to the histogram, False otherwise.
   */
  finishKeyed(histogramId, key, obj, canceledOkay) {
    return TelemetryStopwatch.finishKeyed(histogramId, key, obj, canceledOkay);
  }

  /**
   * Log a value to a histogram.
   *
   * @param  {String} histogramId
   *         Histogram in which the data is to be stored.
   */
  getHistogramById(histogramId) {
    let histogram = null;

    if (histogramId) {
      try {
        histogram = Services.telemetry.getHistogramById(histogramId);
      } catch (e) {
        dump(`Warning: An attempt was made to write to the ${histogramId} ` +
            `histogram, which is not defined in Histograms.json\n` +
            `CALLER: ${getCaller()}`);
      }
    }

    return histogram || {
      add: () => {}
    };
  }

  /**
   * Get a keyed histogram.
   *
   * @param  {String} histogramId
   *         Histogram in which the data is to be stored.
   */
  getKeyedHistogramById(histogramId) {
    let histogram = null;

    if (histogramId) {
      try {
        histogram = Services.telemetry.getKeyedHistogramById(histogramId);
      } catch (e) {
        dump(`Warning: An attempt was made to write to the ${histogramId} ` +
             `histogram, which is not defined in Histograms.json\n` +
             `CALLER: ${getCaller()}`);
      }
    }
    return histogram || {
      add: () => {}
    };
  }

  /**
   * Log a value to a scalar.
   *
   * @param  {String} scalarId
   *         Scalar in which the data is to be stored.
   * @param  value
   *         Value to store.
   */
  scalarSet(scalarId, value) {
    if (!scalarId) {
      return;
    }

    try {
      if (isNaN(value) && typeof value !== "boolean") {
        dump(`Warning: An attempt was made to write a non-numeric and ` +
             `non-boolean value ${value} to the ${scalarId} scalar. Only ` +
             `numeric and boolean values are allowed.\n` +
             `CALLER: ${getCaller()}`);

        return;
      }
      Services.telemetry.scalarSet(scalarId, value);
    } catch (e) {
      dump(`Warning: An attempt was made to write to the ${scalarId} ` +
           `scalar, which is not defined in Scalars.yaml\n` +
           `CALLER: ${getCaller()}`);
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
  scalarAdd(scalarId, value) {
    if (!scalarId) {
      return;
    }

    try {
      if (isNaN(value)) {
        dump(`Warning: An attempt was made to write a non-numeric value ` +
             `${value} to the ${scalarId} scalar. Only numeric values are ` +
             `allowed.\n` +
             `CALLER: ${getCaller()}`);

        return;
      }
      Services.telemetry.scalarAdd(scalarId, value);
    } catch (e) {
      dump(`Warning: An attempt was made to write to the ${scalarId} ` +
           `scalar, which is not defined in Scalars.yaml\n` +
           `CALLER: ${getCaller()}`);
    }
  }

  /**
   * Log a value to a keyed scalar.
   *
   * @param  {String} scalarId
   *         Scalar in which the data is to be stored.
   * @param  {String} key
   *         The key within the  scalar.
   * @param  value
   *         Value to store.
   */
  keyedScalarSet(scalarId, key, value) {
    if (!scalarId) {
      return;
    }

    try {
      if (isNaN(value) && typeof value !== "boolean") {
        dump(`Warning: An attempt was made to write a non-numeric and ` +
             `non-boolean value ${value} to the ${scalarId} scalar. Only ` +
             `numeric and boolean values are allowed.\n` +
             `CALLER: ${getCaller()}`);

        return;
      }
      Services.telemetry.keyedScalarSet(scalarId, key, value);
    } catch (e) {
      dump(`Warning: An attempt was made to write to the ${scalarId} ` +
           `scalar, which is not defined in Scalars.yaml\n` +
           `CALLER: ${getCaller()}`);
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
  keyedScalarAdd(scalarId, key, value) {
    if (!scalarId) {
      return;
    }

    try {
      if (isNaN(value)) {
        dump(`Warning: An attempt was made to write a non-numeric value ` +
             `${value} to the ${scalarId} scalar. Only numeric values are ` +
             `allowed.\n` +
             `CALLER: ${getCaller()}`);

        return;
      }
      Services.telemetry.keyedScalarAdd(scalarId, key, value);
    } catch (e) {
      dump(`Warning: An attempt was made to write to the ${scalarId} ` +
           `scalar, which is not defined in Scalars.yaml\n` +
           `CALLER: ${getCaller()}`);
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
                      `properties.\n` +
                      `CALLER: ${getCaller()}`);
    }

    PENDING_EVENTS.set(sig, {
      extra: {},
      expected: new Set(expected)
    });

    const props = PENDING_EVENT_PROPERTIES.get(sig);
    if (props) {
      for (const [name, val] of Object.entries(props)) {
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
      const props = PENDING_EVENT_PROPERTIES.get(sig);

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
                      `signature "${sig}"\n` +
                      `CALLER: ${getCaller()}`);
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
    for (const [key, val] of Object.entries(pendingObject)) {
      this.addEventProperty(category, method, object, value, key, val);
    }
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
   * @param {String|null} [value]
   *        Optional telemetry event value (a user defined value, providing
   *        context for the event) e.g. "console"
   * @param {Object} [extra]
   *        Optional telemetry event extra object containing the properties that
   *        will be sent with the event e.g.
   *        {
   *          host: "bottom",
   *          width: "1024"
   *        }
   */
  recordEvent(category, method, object, value = null, extra = null) {
    // Only string values are allowed so cast all values to strings.
    if (extra) {
      for (let [name, val] of Object.entries(extra)) {
        val = val + "";
        extra[name] = val;

        if (val.length > 80) {
          const sig = `${category},${method},${object},${value}`;

          throw new Error(`The property "${name}" was added to a telemetry ` +
                          `event with the signature ${sig} but it's value ` +
                          `"${val}" is longer than the maximum allowed length ` +
                          `of 80 characters\n` +
                          `CALLER: ${getCaller()}`);
        }
      }
    }
    Services.telemetry.recordEvent(category, method, object, value, extra);
  }

  /**
   * Sends telemetry pings to indicate that a tool has been opened.
   *
   * @param {String} id
   *        The ID of the tool opened.
   *
   * NOTE: This method is designed for tools that send multiple probes on open,
   *       one of those probes being a counter and the other a timer. If you
   *       only have one probe you should be using another method.
   */
  toolOpened(id) {
    const charts = getChartsFromToolId(id);

    if (charts.timerHist) {
      this.start(charts.timerHist, this);
    }
    if (charts.countHist) {
      this.getHistogramById(charts.countHist).add(true);
    }
    if (charts.countScalar) {
      this.scalarAdd(charts.countScalar, 1);
    }
  }

  /**
   * Sends telemetry pings to indicate that a tool has been closed.
   *
   * @param {String} id
   *        The ID of the tool opened.
   *
   * NOTE: This method is designed for tools that send multiple probes on open,
   *       one of those probes being a counter and the other a timer. If you
   *       only have one probe you should be using another method.
   */
  toolClosed(id) {
    const charts = getChartsFromToolId(id);

    if (charts.timerHist) {
      this.finish(charts.timerHist, this);
    }
  }
}

/**
 * Returns the telemetry charts for a specific tool.
 *
 * @param {String} id
 *        The ID of the tool that has been opened.
 *
 */
function getChartsFromToolId(id) {
  if (!id) {
    return null;
  }

  const lowerCaseId = id.toLowerCase();

  let timerHist = null;
  let countHist = null;
  let countScalar = null;

  id = id.toUpperCase();

  if (id === "PERFORMANCE") {
    id = "JSPROFILER";
  }
  if (id === "NEWANIMATIONINSPECTOR") {
    id = "ANIMATIONINSPECTOR";
  }

  switch (id) {
    case "ABOUTDEBUGGING":
    case "ANIMATIONINSPECTOR":
    case "BROWSERCONSOLE":
    case "CANVASDEBUGGER":
    case "COMPUTEDVIEW":
    case "DEVELOPERTOOLBAR":
    case "DOM":
    case "FONTINSPECTOR":
    case "INSPECTOR":
    case "JSBROWSERDEBUGGER":
    case "JSDEBUGGER":
    case "JSPROFILER":
    case "LAYOUTVIEW":
    case "MEMORY":
    case "NETMONITOR":
    case "OPTIONS":
    case "PAINTFLASHING":
    case "RESPONSIVE":
    case "RULEVIEW":
    case "SCRATCHPAD":
    case "SHADEREDITOR":
    case "STORAGE":
    case "STYLEEDITOR":
    case "TOOLBOX":
    case "WEBAUDIOEDITOR":
    case "WEBCONSOLE":
    case "WEBIDE":
      timerHist = `DEVTOOLS_${id}_TIME_ACTIVE_SECONDS`;
      countHist = `DEVTOOLS_${id}_OPENED_COUNT`;
      break;
    case "ACCESSIBILITY":
    case "APPLICATION":
      timerHist = `DEVTOOLS_${id}_TIME_ACTIVE_SECONDS`;
      countScalar = `devtools.${lowerCaseId}.opened_count`;
      break;
    case "ACCESSIBILITY_PICKER":
      timerHist = `DEVTOOLS_${id}_TIME_ACTIVE_SECONDS`;
      countScalar = `devtools.accessibility.picker_used_count`;
      break;
    default:
      timerHist = `DEVTOOLS_CUSTOM_TIME_ACTIVE_SECONDS`;
      countHist = `DEVTOOLS_CUSTOM_OPENED_COUNT`;
  }

  if (!timerHist || (!countHist && !countScalar)) {
    throw new Error(`getChartsFromToolId cannot be called without a timer ` +
                    `histogram and either a count histogram or count scalar.`);
  }

  return {
    timerHist: timerHist,
    countHist: countHist,
    countScalar: countScalar
  };
}

/**
 * Displays the first caller and calling line outside of this file in the
 * event of an error. This is the line that made the call that produced the
 * error.
 */
function getCaller() {
  return getNthPathExcluding(0, "/telemetry.js");
}

module.exports = Telemetry;
