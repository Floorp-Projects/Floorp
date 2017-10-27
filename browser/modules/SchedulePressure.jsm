/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SchedulePressure"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStopwatch",
  "resource://gre/modules/TelemetryStopwatch.jsm");
XPCOMUtils.defineLazyPreferenceGetter(this, "SCHEDULE_PRESSURE_ENABLED",
  "browser.schedulePressure.enabled", true);
XPCOMUtils.defineLazyPreferenceGetter(this, "TIMEOUT_AMOUNT",
  "browser.schedulePressure.timeoutMs", 1000);

/**
 * The SchedulePressure object provides the ability to alter
 * the behavior of a program based on the idle activity of the
 * host machine.
 */
this.SchedulePressure = {
  _idleCallbackWeakMap: new WeakMap(),
  _setTimeoutWeakMap: new WeakMap(),
  _telemetryCallbackWeakMap: new WeakMap(),

  _createTimeoutFn(window, callbackFn) {
    return () => {
      if (window.closed) {
        TelemetryStopwatch.cancel("FX_SCHEDULE_PRESSURE_IDLE_SAMPLE_MS", window);
        this._telemetryCallbackWeakMap.delete(window);
        return;
      }
      let nextCallbackId = window.requestIdleCallback(callbackFn, {timeout: TIMEOUT_AMOUNT});
      this._idleCallbackWeakMap.set(window, nextCallbackId);

      // Don't create another timeout-less idle callback if the first
      // one hasn't completed yet.
      if (!this._telemetryCallbackWeakMap.has(window) &&
          TelemetryStopwatch.start("FX_SCHEDULE_PRESSURE_IDLE_SAMPLE_MS", window)) {
        let telemetryCallbackId = window.requestIdleCallback(() => {
          TelemetryStopwatch.finish("FX_SCHEDULE_PRESSURE_IDLE_SAMPLE_MS", window);
          this._telemetryCallbackWeakMap.delete(window);
        });
        this._telemetryCallbackWeakMap.set(window, telemetryCallbackId);
      }
    }
  },

  /**
   * Starts an interval timeout that periodically waits for
   * an idle callback. If the idle callback fails to get called
   * within the timeout specified by TIMEOUT_AMOUNT, the
   * highPressureFn callback will get called. Otherwise the
   * lowPressureFn callback will get called.
   *
   * @param  window
   *         The DOM window of the requestee.
   * @param  options
   *           highPressureFn
   *           A function that will be called when the idle callback
   *           fails to be called within the time specified by TIMEOUT_AMOUNT.
   *           Returning an object with property of `continueMonitoring` set
   *           to `false` will prevent further monitoring.
   *           lowPressureFn
   *           A function that will be called when the idle callback
   *           gets called within the time specified by TIMEOUT_AMOUNT.
   *           Returning an object with property of `continueMonitoring` set
   *           to `false` will prevent further monitoring.
   */
  startMonitoring(window, {highPressureFn, lowPressureFn}) {
    if (!SCHEDULE_PRESSURE_ENABLED ||
        this._setTimeoutWeakMap.has(window) ||
        this._idleCallbackWeakMap.has(window)) {
      return;
    }

    let callbackFn = idleDeadline => {
      if (window.closed) {
        return;
      }

      let result;
      if (idleDeadline.didTimeout) {
        try {
          result = highPressureFn();
        } catch (ex) {}
      } else {
        try {
          result = lowPressureFn();
        } catch (ex) {}
      }

      if (result && !result.continueMonitoring) {
        return;
      }

      this._setTimeoutWeakMap.set(window,
        window.setTimeout(this._createTimeoutFn(window, callbackFn), TIMEOUT_AMOUNT));
    };

    this._setTimeoutWeakMap.set(window,
      window.setTimeout(this._createTimeoutFn(window, callbackFn), TIMEOUT_AMOUNT));
  },

  /**
   * Stops the interval timeout that periodically waits for
   * an idle callback.
   *
   * @param  window
   *         The DOM window of the requestee.
   */
  stopMonitoring(window) {
    function removeFromMapAndCancelTimeout(map, cancelFn) {
      if (map.has(window)) {
        cancelFn(map.get(window));
        map.delete(window);
      }
    }
    removeFromMapAndCancelTimeout(this._setTimeoutWeakMap, window.clearTimeout);
    removeFromMapAndCancelTimeout(this._idleCallbackWeakMap, window.cancelIdleCallback);
    removeFromMapAndCancelTimeout(this._telemetryCallbackWeakMap, window.cancelIdleCallback);
  },
};
