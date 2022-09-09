/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is the telemetry module to report metrics for tools.
 *
 * Comprehensive documentation is in docs/frontend/telemetry.md
 */

"use strict";

const TelemetryStopwatch = require("TelemetryStopwatch");
const { getNthPathExcluding } = require("devtools/shared/platform/stack");
const {
  TelemetryEnvironment,
} = require("resource://gre/modules/TelemetryEnvironment.jsm");
const WeakMapMap = require("devtools/client/shared/WeakMapMap");

const CATEGORY = "devtools.main";

// Object to be shared among all instances.
const PENDING_EVENT_PROPERTIES = new WeakMapMap();
const PENDING_EVENTS = new WeakMapMap();

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
    this.addEventProperties = this.addEventProperties.bind(this);
    this.toolOpened = this.toolOpened.bind(this);
    this.toolClosed = this.toolClosed.bind(this);
  }

  get osNameAndVersion() {
    const osInfo = TelemetryEnvironment.currentEnvironment.system.os;

    if (!osInfo) {
      return "Unknown OS";
    }

    let osVersion = `${osInfo.name} ${osInfo.version}`;

    if (osInfo.windowsBuildNumber) {
      osVersion += `.${osInfo.windowsBuildNumber}`;
    }

    return osVersion;
  }

  /**
   * Time since the system wide epoch. This is not a monotonic timer but
   * can be used across process boundaries.
   */
  msSystemNow() {
    return Services.telemetry.msSystemNow();
  }

  /**
   * The number of milliseconds since process start using monotonic
   * timestamps (unaffected by system clock changes).
   */
  msSinceProcessStart() {
    return Services.telemetry.msSinceProcessStart();
  }

  /**
   * Starts a timer associated with a telemetry histogram. The timer can be
   * directly associated with a histogram, or with a pair of a histogram and
   * an object.
   *
   * @param {String} histogramId
   *        A string which must be a valid histogram name.
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
   * @param {Object}  [options.inSeconds=false]
   *        Record elapsed time for this histogram in seconds instead of
   *        milliseconds. Defaults to false.
   * @returns {Boolean}
   *          True if the timer was successfully started, false otherwise. If a
   *          timer already exists, it can't be started again.
   */
  start(histogramId, obj, { inSeconds } = {}) {
    if (TelemetryStopwatch.running(histogramId, obj)) {
      return false;
    }

    return TelemetryStopwatch.start(histogramId, obj, { inSeconds });
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
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
   * @param {Object}  [options.inSeconds=false]
   *        Record elapsed time for this histogram in seconds instead of
   *        milliseconds. Defaults to false.
   *
   * @returns {Boolean}
   *          True if the timer was successfully started, false otherwise. If a
   *          timer already exists, it can't be started again, and the existing
   *          one will be cleared in order to avoid measurements errors.
   */
  startKeyed(histogramId, key, obj, { inSeconds } = {}) {
    return TelemetryStopwatch.startKeyed(histogramId, key, obj, { inSeconds });
  }

  /**
   * Stops the timer associated with the given histogram (and object),
   * calculates the time delta between start and finish, and adds the value
   * to the histogram.
   *
   * @param {String} histogramId
   *        A string which must be a valid histogram name.
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
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
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
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
        dump(
          `Warning: An attempt was made to write to the ${histogramId} ` +
            `histogram, which is not defined in Histograms.json\n` +
            `CALLER: ${getCaller()}`
        );
      }
    }

    return (
      histogram || {
        add: () => {},
      }
    );
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
        dump(
          `Warning: An attempt was made to write to the ${histogramId} ` +
            `histogram, which is not defined in Histograms.json\n` +
            `CALLER: ${getCaller()}`
        );
      }
    }
    return (
      histogram || {
        add: () => {},
      }
    );
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
        dump(
          `Warning: An attempt was made to write a non-numeric and ` +
            `non-boolean value ${value} to the ${scalarId} scalar. Only ` +
            `numeric and boolean values are allowed.\n` +
            `CALLER: ${getCaller()}`
        );

        return;
      }
      Services.telemetry.scalarSet(scalarId, value);
    } catch (e) {
      dump(
        `Warning: An attempt was made to write to the ${scalarId} ` +
          `scalar, which is not defined in Scalars.yaml\n` +
          `CALLER: ${getCaller()}`
      );
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
        dump(
          `Warning: An attempt was made to write a non-numeric value ` +
            `${value} to the ${scalarId} scalar. Only numeric values are ` +
            `allowed.\n` +
            `CALLER: ${getCaller()}`
        );

        return;
      }
      Services.telemetry.scalarAdd(scalarId, value);
    } catch (e) {
      dump(
        `Warning: An attempt was made to write to the ${scalarId} ` +
          `scalar, which is not defined in Scalars.yaml\n` +
          `CALLER: ${getCaller()}`
      );
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
        dump(
          `Warning: An attempt was made to write a non-numeric and ` +
            `non-boolean value ${value} to the ${scalarId} scalar. Only ` +
            `numeric and boolean values are allowed.\n` +
            `CALLER: ${getCaller()}`
        );

        return;
      }
      Services.telemetry.keyedScalarSet(scalarId, key, value);
    } catch (e) {
      dump(
        `Warning: An attempt was made to write to the ${scalarId} ` +
          `scalar, which is not defined in Scalars.yaml\n` +
          `CALLER: ${getCaller()}`
      );
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
        dump(
          `Warning: An attempt was made to write a non-numeric value ` +
            `${value} to the ${scalarId} scalar. Only numeric values are ` +
            `allowed.\n` +
            `CALLER: ${getCaller()}`
        );

        return;
      }
      Services.telemetry.keyedScalarAdd(scalarId, key, value);
    } catch (e) {
      dump(
        `Warning: An attempt was made to write to the ${scalarId} ` +
          `scalar, which is not defined in Scalars.yaml\n` +
          `CALLER: ${getCaller()}`
      );
    }
  }

  /**
   * Event telemetry is disabled by default. Use this method to enable or
   * disable it.
   *
   * @param {Boolean} enabled
   *        Enabled: true or false.
   */
  setEventRecordingEnabled(enabled) {
    return Services.telemetry.setEventRecordingEnabled(CATEGORY, enabled);
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
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
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
  preparePendingEvent(obj, method, object, value, expected = []) {
    const sig = `${method},${object},${value}`;

    if (expected.length === 0) {
      throw new Error(
        `preparePendingEvent() was called without any expected ` +
          `properties.\n` +
          `CALLER: ${getCaller()}`
      );
    }

    const data = {
      extra: {},
      expected: new Set(expected),
    };

    PENDING_EVENTS.set(obj, sig, data);

    const props = PENDING_EVENT_PROPERTIES.get(obj, sig);
    if (props) {
      for (const [name, val] of Object.entries(props)) {
        this.addEventProperty(obj, method, object, value, name, val);
      }
      PENDING_EVENT_PROPERTIES.delete(obj, sig);
    }
  }

  /**
   * Adds an expected property for either a current or future pending event.
   * This means that if preparePendingEvent() is called before or after sending
   * the event properties they will automatically added to the event.
   *
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
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
  addEventProperty(
    obj,
    method,
    object,
    value,
    pendingPropName,
    pendingPropValue
  ) {
    const sig = `${method},${object},${value}`;
    const events = PENDING_EVENTS.get(obj, sig);

    // If the pending event has not been created add the property to the pending
    // list.
    if (!events) {
      const props = PENDING_EVENT_PROPERTIES.get(obj, sig);

      if (props) {
        props[pendingPropName] = pendingPropValue;
      } else {
        PENDING_EVENT_PROPERTIES.set(obj, sig, {
          [pendingPropName]: pendingPropValue,
        });
      }
      return;
    }

    const { expected, extra } = events;

    if (expected.has(pendingPropName)) {
      extra[pendingPropName] = pendingPropValue;

      if (expected.size === Object.keys(extra).length) {
        this._sendPendingEvent(obj, method, object, value);
      }
    } else {
      // The property was not expected, warn and bail.
      throw new Error(
        `An attempt was made to add the unexpected property ` +
          `"${pendingPropName}" to a telemetry event with the ` +
          `signature "${sig}"\n` +
          `CALLER: ${getCaller()}`
      );
    }
  }

  /**
   * Adds expected properties for either a current or future pending event.
   * This means that if preparePendingEvent() is called before or after sending
   * the event properties they will automatically added to the event.
   *
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
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
  addEventProperties(obj, method, object, value, pendingObject) {
    for (const [key, val] of Object.entries(pendingObject)) {
      this.addEventProperty(obj, method, object, value, key, val);
    }
  }

  /**
   * A private method that is not to be used externally. This method is used to
   * prepare a pending telemetry event for sending and then send it via
   * recordEvent().
   *
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
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
  _sendPendingEvent(obj, method, object, value) {
    const sig = `${method},${object},${value}`;
    const { extra } = PENDING_EVENTS.get(obj, sig);

    PENDING_EVENTS.delete(obj, sig);
    PENDING_EVENT_PROPERTIES.delete(obj, sig);
    this.recordEvent(method, object, value, extra);
  }

  /**
   * Send a telemetry event.
   *
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
  recordEvent(method, object, value = null, extra = null) {
    // Only string values are allowed so cast all values to strings.
    if (extra) {
      for (let [name, val] of Object.entries(extra)) {
        val = val + "";

        if (val.length > 80) {
          const sig = `${method},${object},${value}`;

          dump(
            `Warning: The property "${name}" was added to a telemetry ` +
              `event with the signature ${sig} but it's value "${val}" is ` +
              `longer than the maximum allowed length of 80 characters.\n` +
              `The property value has been trimmed to 80 characters before ` +
              `sending.\nCALLER: ${getCaller()}`
          );

          val = val.substring(0, 80);
        }

        extra[name] = val;
      }
    }
    Services.telemetry.recordEvent(CATEGORY, method, object, value, extra);
  }

  /**
   * Sends telemetry pings to indicate that a tool has been opened.
   *
   * @param {String} id
   *        The ID of the tool opened.
   * @param {String} sessionId
   *        Toolbox session id used when we need to ensure a tool really has a
   *        timer before calculating a delta.
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
   *
   * NOTE: This method is designed for tools that send multiple probes on open,
   *       one of those probes being a counter and the other a timer. If you
   *       only have one probe you should be using another method.
   */
  toolOpened(id, sessionId, obj) {
    if (typeof sessionId === "undefined") {
      throw new Error(`toolOpened called without a sessionId parameter.`);
    }

    const charts = getChartsFromToolId(id);

    if (!charts) {
      return;
    }

    if (charts.useTimedEvent) {
      this.preparePendingEvent(obj, "tool_timer", id, null, [
        "os",
        "time_open",
        "session_id",
      ]);
      this.addEventProperty(
        obj,
        "tool_timer",
        id,
        null,
        "time_open",
        this.msSystemNow()
      );
    }
    if (charts.timerHist) {
      this.start(charts.timerHist, obj, { inSeconds: true });
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
   * @param {String} sessionId
   *        Toolbox session id.
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
   *
   * NOTE: This method is designed for tools that send multiple probes on open,
   *       one of those probes being a counter and the other a timer. If you
   *       only have one probe you should be using another method.
   */
  toolClosed(id, sessionId, obj) {
    if (typeof sessionId === "undefined") {
      throw new Error(`toolClosed called without a sessionId parameter.`);
    }

    const charts = getChartsFromToolId(id);

    if (!charts) {
      return;
    }

    if (charts.useTimedEvent) {
      const sig = `tool_timer,${id},null`;
      const event = PENDING_EVENTS.get(obj, sig);
      const time = this.msSystemNow() - event.extra.time_open;

      this.addEventProperties(obj, "tool_timer", id, null, {
        time_open: time,
        os: this.osNameAndVersion,
        session_id: sessionId,
      });
    }

    if (charts.timerHist) {
      this.finish(charts.timerHist, obj, false);
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
// eslint-disable-next-line complexity
function getChartsFromToolId(id) {
  if (!id) {
    return null;
  }

  const lowerCaseId = id.toLowerCase();

  let useTimedEvent = null;
  let timerHist = null;
  let countHist = null;
  let countScalar = null;

  id = id.toUpperCase();

  if (id === "PERFORMANCE") {
    id = "JSPROFILER";
  }

  switch (id) {
    case "ABOUTDEBUGGING":
    case "BROWSERCONSOLE":
    case "DOM":
    case "INSPECTOR":
    case "JSBROWSERDEBUGGER":
    case "JSDEBUGGER":
    case "JSPROFILER":
    case "MEMORY":
    case "NETMONITOR":
    case "OPTIONS":
    case "RESPONSIVE":
    case "STORAGE":
    case "STYLEEDITOR":
    case "TOOLBOX":
    case "WEBCONSOLE":
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
    case "CHANGESVIEW":
      useTimedEvent = true;
      timerHist = `DEVTOOLS_${id}_TIME_ACTIVE_SECONDS`;
      countScalar = `devtools.${lowerCaseId}.opened_count`;
      break;
    case "ANIMATIONINSPECTOR":
    case "COMPATIBILITYVIEW":
    case "COMPUTEDVIEW":
    case "FONTINSPECTOR":
    case "LAYOUTVIEW":
    case "RULEVIEW":
      useTimedEvent = true;
      timerHist = `DEVTOOLS_${id}_TIME_ACTIVE_SECONDS`;
      countHist = `DEVTOOLS_${id}_OPENED_COUNT`;
      break;
    case "FLEXBOX_HIGHLIGHTER":
    case "GRID_HIGHLIGHTER":
      timerHist = `DEVTOOLS_${id}_TIME_ACTIVE_SECONDS`;
      break;
    default:
      timerHist = `DEVTOOLS_CUSTOM_TIME_ACTIVE_SECONDS`;
      countHist = `DEVTOOLS_CUSTOM_OPENED_COUNT`;
  }

  return {
    useTimedEvent,
    timerHist,
    countHist,
    countScalar,
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
