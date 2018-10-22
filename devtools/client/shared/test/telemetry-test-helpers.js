/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* global is ok registerCleanupFunction Services */

"use strict";

// We try to avoid polluting the global scope as far as possible by defining
// constants in the methods that use them because this script is not sandboxed
// meaning that it is loaded via Services.scriptloader.loadSubScript()

class TelemetryHelpers {
  constructor() {
    this.oldCanRecord = Services.telemetry.canRecordExtended;
    this.generateTelemetryTests = this.generateTelemetryTests.bind(this);
    registerCleanupFunction(this.stopTelemetry.bind(this));
  }

  /**
   * Allow collection of extended telemetry data.
   */
  startTelemetry() {
    Services.telemetry.canRecordExtended = true;
  }

  /**
   * Clear all telemetry types.
   */
  stopTelemetry() {
    Services.telemetry.canRecordExtended = this.oldCanRecord;

    // Clear histograms, scalars and Telemetry Events.
    this.clearHistograms(Services.telemetry.snapshotHistograms);
    this.clearHistograms(Services.telemetry.snapshotKeyedHistograms);
    Services.telemetry.clearScalars();
    Services.telemetry.clearEvents();
  }

  /**
   * Clears both OPTIN and OPTOUT versions of Telemetry Histograms.
   *
   * @param {Function} snapshotFunc
   *        The function used to take the snapshot. This can be one of the
   *        following:
   *          - Services.telemetry.snapshotHistograms
   *          - Services.telemetry.snapshotKeyedHistograms
   *
   *        `snapshotFunc(OPTIN, true, true)` should clear the histograms but this
   *        only deletes seemingly random histograms, hence this method.
   */
  clearHistograms(snapshotFunc) {
    // Although most of our Telemetry probes are OPTOUT, OPTIN includes all OPTIN
    // *and* OPTOUT data.
    const OPTIN = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN;
    const OPTOUT = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT;
    const tel = Services.telemetry;

    for (const optInOut of [OPTIN, OPTOUT]) {
      const snapshot = snapshotFunc(optInOut, true, false).parent;
      const histKeys = Object.keys(snapshot);

      for (const getHistogram of [tel.getHistogramById, tel.getKeyedHistogramById]) {
        for (const key of histKeys) {
          try {
            getHistogram(key).clear();
          } catch (e) {
            // Some histograms may have already been cleaned up by the system so we
            // swallow the "histogram does not exist" error silently here.
          }
        }
      }
    }
  }

  /**
   * Check the value of a given telemetry histogram.
   *
   * @param  {String} histId
   *         Histogram id
   * @param  {String} key
   *         Keyed histogram key
   * @param  {Array|Number} expected
   *         Expected value
   * @param  {String} checkType
   *         "array" (default) - Check that an array matches the histogram data.
   *         "hasentries"  - For non-enumerated linear and exponential
   *                             histograms. This checks for at least one entry.
   *         "scalar" - Telemetry type is a scalar.
   *         "keyedscalar" - Telemetry type is a keyed scalar.
   */
  checkTelemetry(histId, key, expected, checkType) {
    // Although most of our Telemetry probes are OPTOUT, OPTIN includes all OPTIN
    // *and* OPTOUT data.
    const OPTIN = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN;

    let actual;
    let msg;

    if (checkType === "array" || checkType === "hasentries") {
      if (key) {
        const keyedHistogram =
          Services.telemetry.getKeyedHistogramById(histId).snapshot();
        const result = keyedHistogram[key];

        if (result) {
          actual = result.counts;
        } else {
          ok(false, `${histId}[${key}] exists`);
          return;
        }
      } else {
        actual = Services.telemetry.getHistogramById(histId).snapshot().counts;
      }
    }

    switch (checkType) {
      case "array":
        msg = key ? `${histId}["${key}"] correct.` : `${histId} correct.`;
        is(JSON.stringify(actual), JSON.stringify(expected), msg);
        break;
      case "hasentries":
        const hasEntry = actual.some(num => num > 0);
        if (key) {
          ok(hasEntry, `${histId}["${key}"] has at least one entry.`);
        } else {
          ok(hasEntry, `${histId} has at least one entry.`);
        }
        break;
      case "scalar":
        const scalars =
          Services.telemetry.snapshotScalars(OPTIN, false).parent;

        is(scalars[histId], expected, `${histId} correct`);
        break;
      case "keyedscalar":
        const keyedScalars =
          Services.telemetry.snapshotKeyedScalars(OPTIN, false).parent;
        const value = keyedScalars[histId][key];

        msg = key ? `${histId}["${key}"] correct.` : `${histId} correct.`;
        is(value, expected, msg);
        break;
    }
  }

  /**
   * Generate telemetry tests. You should call generateTelemetryTests("DEVTOOLS_")
   * from your result checking code in telemetry tests. It logs checkTelemetry
   * calls for all changed telemetry values.
   *
   * @param  {String} prefix
   *         Optionally limits results to histogram ids starting with prefix.
   */
  generateTelemetryTests(prefix = "") {
    // Although most of our Telemetry probes are OPTOUT, OPTIN includes all OPTIN
    // *and* OPTOUT data.
    const OPTIN = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN;

    // Get all histograms and scalars
    const histograms =
      Services.telemetry.snapshotHistograms(OPTIN, true, false).parent;
    const keyedHistograms =
      Services.telemetry.snapshotKeyedHistograms(OPTIN, true, false).parent;
    const scalars =
      Services.telemetry.snapshotScalars(OPTIN, false).parent;
    const keyedScalars =
      Services.telemetry.snapshotKeyedScalars(OPTIN, false).parent;
    const allHistograms = Object.assign({},
                                        histograms,
                                        keyedHistograms,
                                        scalars,
                                        keyedScalars);
    // Get all keys
    const histIds = Object.keys(allHistograms)
                          .filter(histId => histId.startsWith(prefix));

    dump("=".repeat(80) + "\n");
    for (const histId of histIds) {
      const snapshot = allHistograms[histId];

      if (histId === histId.toLowerCase()) {
        if (typeof snapshot === "object") {
          // Keyed Scalar
          const keys = Object.keys(snapshot);

          for (const key of keys) {
            const value = snapshot[key];

            dump(`checkTelemetry("${histId}", "${key}", ${value}, "keyedscalar");\n`);
          }
        } else {
          // Scalar
          dump(`checkTelemetry("${histId}", "", ${snapshot}, "scalar");\n`);
        }
      } else if (typeof snapshot.histogram_type !== "undefined" &&
                typeof snapshot.counts !== "undefined") {
        // Histogram
        const actual = snapshot.counts;

        this.displayDataFromHistogramSnapshot(snapshot, "", histId, actual);
      } else {
        // Keyed Histogram
        const keys = Object.keys(snapshot);

        for (const key of keys) {
          const value = snapshot[key];
          const actual = value.counts;

          this.displayDataFromHistogramSnapshot(value, key, histId, actual);
        }
      }
    }
    dump("=".repeat(80) + "\n");
  }

  /**
   * Generates the inner contents of a test's checkTelemetry() method.
   *
   * @param {HistogramSnapshot} snapshot
   *        A snapshot of a telemetry chart obtained via snapshotHistograms or
   *        similar.
   * @param {String} key
   *        Only used for keyed histograms. This is the key we are interested in
   *        checking.
   * @param {String} histId
   *        The histogram ID.
   * @param {Array|String|Boolean} actual
   *        The value of the histogram data.
   */
  displayDataFromHistogramSnapshot(snapshot, key, histId, actual) {
    key = key ? `"${key}"` : `""`;

    switch (snapshot.histogram_type) {
      case Services.telemetry.HISTOGRAM_EXPONENTIAL:
      case Services.telemetry.HISTOGRAM_LINEAR:
        let total = 0;
        for (const val of actual) {
          total += val;
        }

        if (histId.endsWith("_ENUMERATED")) {
          if (total > 0) {
            actual = actual.toSource();
            dump(`checkTelemetry("${histId}", ${key}, ${actual}, "array");\n`);
          }
          return;
        }

        dump(`checkTelemetry("${histId}", ${key}, null, "hasentries");\n`);
        break;
      case Services.telemetry.HISTOGRAM_BOOLEAN:
        actual = actual.toSource();

        if (actual !== "[0, 0, 0]") {
          dump(`checkTelemetry("${histId}", ${key}, ${actual}, "array");\n`);
        }
        break;
      case Services.telemetry.HISTOGRAM_FLAG:
        actual = actual.toSource();

        if (actual !== "[1, 0, 0]") {
          dump(`checkTelemetry("${histId}", ${key}, ${actual}, "array");\n`);
        }
        break;
      case Services.telemetry.HISTOGRAM_COUNT:
        actual = actual.toSource();

        dump(`checkTelemetry("${histId}", ${key}, ${actual}, "array");\n`);
        break;
    }
  }
}

// "exports"... because this is a helper and not imported via require we need to
// expose the three main methods that should be used by tests. The reason this
// is not imported via require is because it needs access to test methods
// (is, ok etc).

/* eslint-disable no-unused-vars */
const telemetryHelpers = new TelemetryHelpers();
const generateTelemetryTests = telemetryHelpers.generateTelemetryTests;
const checkTelemetry = telemetryHelpers.checkTelemetry;
const startTelemetry = telemetryHelpers.startTelemetry;
/* eslint-enable no-unused-vars */
