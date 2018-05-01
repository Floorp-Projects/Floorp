/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */
/* import-globals-from shared-head.js */
/* import-globals-from telemetry-test-helpers.js */

"use strict";

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js", this);

const {DOMHelpers} = ChromeUtils.import("resource://devtools/client/shared/DOMHelpers.jsm", {});
const {Hosts} = require("devtools/client/framework/toolbox-hosts");

const TEST_URI_ROOT = "http://example.com/browser/devtools/client/shared/test/";
const OPTIONS_VIEW_URL = TEST_URI_ROOT + "doc_options-view.xul";

function catchFail(func) {
  return function() {
    try {
      return func.apply(null, arguments);
    } catch (ex) {
      ok(false, ex);
      console.error(ex);
      finish();
      throw ex;
    }
  };
}

/**
 * Polls a given function waiting for the given value.
 *
 * @param object options
 *        Options object with the following properties:
 *        - validator
 *        A validator function that should return the expected value. This is
 *        called every few milliseconds to check if the result is the expected
 *        one. When the returned result is the expected one, then the |success|
 *        function is called and polling stops. If |validator| never returns
 *        the expected value, then polling timeouts after several tries and
 *        a failure is recorded - the given |failure| function is invoked.
 *        - success
 *        A function called when the validator function returns the expected
 *        value.
 *        - failure
 *        A function called if the validator function timeouts - fails to return
 *        the expected value in the given time.
 *        - name
 *        Name of test. This is used to generate the success and failure
 *        messages.
 *        - timeout
 *        Timeout for validator function, in milliseconds. Default is 5000 ms.
 *        - value
 *        The expected value. If this option is omitted then the |validator|
 *        function must return a trueish value.
 *        Each of the provided callback functions will receive two arguments:
 *        the |options| object and the last value returned by |validator|.
 */
function waitForValue(options) {
  let start = Date.now();
  let timeout = options.timeout || 5000;
  let lastValue;

  function wait(validatorFn, successFn, failureFn) {
    if ((Date.now() - start) > timeout) {
      // Log the failure.
      ok(false, "Timed out while waiting for: " + options.name);
      let expected = "value" in options ?
                     "'" + options.value + "'" :
                     "a trueish value";
      info("timeout info :: got '" + lastValue + "', expected " + expected);
      failureFn(options, lastValue);
      return;
    }

    lastValue = validatorFn(options, lastValue);
    let successful = "value" in options ?
                      lastValue == options.value :
                      lastValue;
    if (successful) {
      ok(true, options.name);
      successFn(options, lastValue);
    } else {
      setTimeout(() => {
        wait(validatorFn, successFn, failureFn);
      }, 100);
    }
  }

  wait(options.validator, options.success, options.failure);
}

function oneTimeObserve(name, callback) {
  return new Promise((resolve) => {
    let func = function() {
      Services.obs.removeObserver(func, name);
      if (callback) {
        callback();
      }
      resolve();
    };
    Services.obs.addObserver(func, name);
  });
}

let createHost =
async function(type = "bottom", src = CHROME_URL_ROOT + "dummy.html") {
  let host = new Hosts[type](gBrowser.selectedTab);
  let iframe = await host.create();

  await new Promise(resolve => {
    let domHelper = new DOMHelpers(iframe.contentWindow);
    iframe.setAttribute("src", src);
    domHelper.onceDOMReady(resolve);
  });

  return [host, iframe.contentWindow, iframe.contentDocument];
};

/**
 * Open and close the toolbox in the current browser tab, several times, waiting
 * some amount of time in between.
 * @param {Number} nbOfTimes
 * @param {Number} usageTime in milliseconds
 * @param {String} toolId
 */
async function openAndCloseToolbox(nbOfTimes, usageTime, toolId) {
  for (let i = 0; i < nbOfTimes; i++) {
    info("Opening toolbox " + (i + 1));
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    await gDevTools.showToolbox(target, toolId);

    // We use a timeout to check the toolbox's active time
    await new Promise(resolve => setTimeout(resolve, usageTime));

    info("Closing toolbox " + (i + 1));
    await gDevTools.closeToolbox(target);
  }
}

/**
 * Synthesize a profile for testing.
 */
function synthesizeProfileForTest(samples) {
  const RecordingUtils = require("devtools/shared/performance/recording-utils");

  samples.unshift({
    time: 0,
    frames: []
  });

  let uniqueStacks = new RecordingUtils.UniqueStacks();
  return RecordingUtils.deflateThread({
    samples: samples,
    markers: []
  }, uniqueStacks);
}

/**
 * Waits until a predicate returns true.
 *
 * @param function predicate
 *        Invoked once in a while until it returns true.
 * @param number interval [optional]
 *        How often the predicate is invoked, in milliseconds.
 */
function waitUntil(predicate, interval = 10) {
  if (predicate()) {
    return Promise.resolve(true);
  }
  return new Promise(resolve => {
    setTimeout(function() {
      waitUntil(predicate).then(() => resolve(true));
    }, interval);
  });
}

/**
 * Show the presets list sidebar in the cssfilter widget popup
 * @param {CSSFilterWidget} widget
 * @return {Promise}
 */
function showFilterPopupPresets(widget) {
  let onRender = widget.once("render");
  widget._togglePresets();
  return onRender;
}

/**
 * Show presets list and create a sample preset with the name and value provided
 * @param  {CSSFilterWidget} widget
 * @param  {string} name
 * @param  {string} value
 * @return {Promise}
 */
let showFilterPopupPresetsAndCreatePreset =
async function(widget, name, value) {
  await showFilterPopupPresets(widget);

  let onRender = widget.once("render");
  widget.setCssValue(value);
  await onRender;

  let footer = widget.el.querySelector(".presets-list .footer");
  footer.querySelector("input").value = name;

  onRender = widget.once("render");
  widget._savePreset({
    preventDefault: () => {}
  });

  await onRender;
};
