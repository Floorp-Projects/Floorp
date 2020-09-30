/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * This is a stub of the DevTools telemetry module and will be replaced by the
 * full version of the file by Webpack for running inside Firefox.
 */

class Telemetry {
  /**
   * Time since the system wide epoch. This is not a monotonic timer but
   * can be used across process boundaries.
   */
  get msSystemNow() {
    return 0;
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
    return true;
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
    return true;
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
   *        fire when a stopwatch is finished after being cancelled.
   *        Defaults to false.
   *
   * @returns {Boolean}
   *          True if the timer was succesfully stopped and the data was added
   *          to the histogram, False otherwise.
   */
  finish(histogramId, obj, canceledOkay) {
    return true;
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
   *        fire when a stopwatch is finished after being cancelled.
   *        Defaults to false.
   *
   * @returns {Boolean}
   *          True if the timer was succesfully stopped and the data was added
   *          to the histogram, False otherwise.
   */
  finishKeyed(histogramId, key, obj, cancelledOkay) {
    return true;
  }

  /**
   * Log a value to a histogram.
   *
   * @param  {String} histogramId
   *         Histogram in which the data is to be stored.
   */
  getHistogramById(histogramId) {
    return {
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
    return {
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
  scalarSet(scalarId, value) {}

  /**
   * Log a value to a count scalar.
   *
   * @param  {String} scalarId
   *         Scalar in which the data is to be stored.
   * @param  value
   *         Value to store.
   */
  scalarAdd(scalarId, value) {}

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
  keyedScalarAdd(scalarId, key, value) {}

  /**
   * Event telemetry is disabled by default. Use this method to enable it for
   * a particular category.
   *
   * @param {Boolean} enabled
   *        Enabled: true or false.
   */
  setEventRecordingEnabled(enabled) {
    return enabled;
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
  preparePendingEvent(obj, method, object, value, expected = []) {}

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
  addEventProperty(obj, method, object, value, pendingPropName, pendingPropValue) {}

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
  addEventProperties(obj, method, object, value, pendingObject) {}

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
  _sendPendingEvent(obj, method, object, value) {}

  /**
   * Send a telemetry event.
   *
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
  recordEvent(method, object, value, extra) {}

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
   */
  toolOpened(id, sessionId, obj) {}

  /**
   * Sends telemetry pings to indicate that a tool has been closed.
   *
   * @param {String} id
   *        The ID of the tool opened.
   */
  toolClosed(id, sessionId, obj) {}
}

module.exports = Telemetry;
