/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Misc. constants
const kInfoHeader = "PERF-TEST | ";
const kDeclareId = "DECLARE ";
const kResultsId = "RESULTS ";

// Mochitest log data format version
const kDataSetVersion = "1";

/*
 * PerfTest - helper library for simple mochitest based performance tests.
 */

var PerfTest = {
  _userStartTime: 0,
  _userStopTime: 0,

  /******************************************************
   * Declare and results
   */

  /*
   * declareTest
   *
   * Declare a test which the graph server will pick up and track.
   * Must be called by every test on startup. Graph server will
   * search for result data between this declaration and the next.
   *
   * @param aUUID string for this particular test, case sensitive.
   * @param aName The name of the test.
   * @param aCategory Top level test calegory. For example 'General',
   * 'Graphics', 'Startup', 'Jim's Tests'.
   * @param aSubCategory (optional) sub category name with aCategory.
   * @param aDescription A detailed description (sentence or two) of
   * what the test does.
   */
  declareTest: function declareTest(aUUID, aName, aCategory, aSubCategory, aDescription) {
    this._uid = aUUID;
    this._print(kDeclareId, this._toJsonStr({
      id: aUUID,
      version: kDataSetVersion,
      name: aName,
      category: aCategory,
      subcategory: aSubCategory,
      description: aDescription,
      buildid: Services.appinfo.appBuildID,
    }));
  },

  /*
   * declareNumericalResult
   *
   * Declare a simple numerical result.
   *
   * @param aValue numerical value to record
   * @param aDescription string describing the value to display on the y axis.
   */
  declareNumericalResult: function declareNumericalResult(aValue, aDescription) {
    this._print(kResultsId, this._toJsonStr({
      id: this._uid,
      version: kDataSetVersion,
      results: {
        r0: {
          value: aValue,
          desc: aDescription
        }
      },
    }));
  },

  /*
   * declareFrameRateResult
   *
   * Declare a frame rate for a result.
   *
   * @param aFrameCount numerical frame count
   * @param aRunMs run time in miliseconds
   * @param aDescription string describing the value to display on the y axis.
   */
  declareFrameRateResult: function declareFrameRateResult(aFrameCount, aRunMs, aDescription) {
    this._print(kResultsId, this._toJsonStr({
      id: this._uid,
      version: kDataSetVersion,
      results: {
        r0: {
          value: (aFrameCount / (aRunMs / 1000.0)),
          desc: aDescription
        }
      },
    }));
  },

  /*
   * declareNumericalResults
   *
   * Declare a set of numerical results.
   *
   * @param aArray an array of datapoint objects of the form:
   *
   *  [ { value: (value), desc: "description/units" }, .. ]
   *
   *  optional values:
   *    shareAxis - the 0 based index of a previous data point this point
   *    should share a y axis with.
   */
  declareNumericalResults: function declareNumericalResults(aArray) {
    let collection = new Object();
    for (let idx = 0; idx < aArray.length; idx++) {
      collection['r' + idx] = { value: aArray[idx].value, desc: aArray[idx].desc };
      if (aArray[idx].shareAxis != undefined) {
        collection['r' + idx].shareAxis = aArray[idx].shareAxis;
      }
    }
    let dataset = {
      id: this._uid,
      version: kDataSetVersion,
      results: collection
    };
    this._print(kResultsId, this._toJsonStr(dataset));
  },

  /******************************************************
   * Perf tests
   */

  perfBoundsCheck: function perfBoundsCheck(aLow, aHigh, aValue, aTestMessage) {
    ok(aValue < aLow || aValue > aHigh, aTestMessage);
  },

  /******************************************************
   * Math utilities
   */

  computeMedian: function computeMedian(aArray, aOptions) {
    aArray.sort(function (a, b) {
      return a - b;
    });
 
    var idx = Math.floor(aArray.length / 2);
 
    if(aArray.length % 2) {
        return aArray[idx];
    } else {
        return (aArray[idx-1] + aArray[idx]) / 2;
    }
  },

  computeAverage: function computeAverage(aArray, aOptions) {
    let idx;
    let count = 0, total = 0;
    let highIdx = -1, lowIdx = -1;
    let high = 0, low = 0;
    if (aOptions.stripOutliers) {
      for (idx = 0; idx < aArray.length; idx++) {
        if (high < aArray[idx]) {
          highIdx = idx;
          high = aArray[idx];
        }
        if (low > aArray[idx]) {
          lowIdx = idx;
          low = aArray[idx];
        }
      }
    }
    for (idx = 0; idx < aArray.length; idx++) {
      if (idx != high && idx != low) {
        total += aArray[idx];
        count++;
      }
    }
    return total / count;
  },

  /******************************************************
   * Internal
   */

  _print: function _print() {
    let str = kInfoHeader;
    for (let idx = 0; idx < arguments.length; idx++) {
      str += arguments[idx];
    }
    info(str);
  },

  _toJsonStr: function _toJsonStr(aTable) {
    return window.JSON.stringify(aTable);
  },
};

/*
 * StopWatch - timing helper
 */

function StopWatch(aStart) {
  if (aStart) {
    this.start();
  }
}

StopWatch.prototype = {
  /*
   * Start timing. Resets existing clock.
   */
  start: function start() {
    this.reset();
    this._userStartTime = window.performance.now();
  },

  /*
   * Stop timing.
   */
  stop: function stop() {
    this._userStopTime = window.performance.now();
    return this.time();
  },

  /*
   * Resets both start and end time.
   */
  reset: function reset() {
    this._userStartTime = this._userStopTime = 0;
  },

  /*
   * Returns the total time ellapsed in milliseconds. Returns zero if
   * no time has been accumulated.
   */
  time: function time() {
    if (!this._userStartTime) {
      return 0;
    }
    if (!this._userStopTime) {
      return (window.performance.now() - this._userStartTime);
    }
    return this._userStopTime - this._userStartTime;
  },
};
