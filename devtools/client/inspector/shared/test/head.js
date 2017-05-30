/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../test/head.js */
"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this);

var {CssRuleView} = require("devtools/client/inspector/rules/rules");
var {getInplaceEditorForSpan: inplaceEditor} =
  require("devtools/client/shared/inplace-editor");
const {getColor: getThemeColor} = require("devtools/client/shared/theme");

const TEST_URL_ROOT =
  "http://example.com/browser/devtools/client/inspector/shared/test/";
const TEST_URL_ROOT_SSL =
  "https://example.com/browser/devtools/client/inspector/shared/test/";
const ROOT_TEST_DIR = getRootDirectory(gTestPath);
const FRAME_SCRIPT_URL = ROOT_TEST_DIR + "doc_frame_script.js";
const STYLE_INSPECTOR_L10N =
      new LocalizationHelper("devtools/shared/locales/styleinspector.properties");

// Clean-up all prefs that might have been changed during a test run
// (safer here because if the test fails, then the pref is never reverted)
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.defaultColorUnit");
});

/**
 * The functions found below are here to ease test development and maintenance.
 * Most of these functions are stateless and will require some form of context
 * (the instance of the current toolbox, or inspector panel for instance).
 *
 * Most of these functions are async too and return promises.
 *
 * All tests should follow the following pattern:
 *
 * add_task(function*() {
 *   yield addTab(TEST_URI);
 *   let {toolbox, inspector} = yield openInspector();
 *   inspector.sidebar.select(viewId);
 *   let view = inspector.getPanel(viewId).view;
 *   yield selectNode("#test", inspector);
 *   yield someAsyncTestFunction(view);
 * });
 *
 * add_task is the way to define the testcase in the test file. It accepts
 * a single generator-function argument.
 * The generator function should yield any async call.
 *
 * There is no need to clean tabs up at the end of a test as this is done
 * automatically.
 *
 * It is advised not to store any references on the global scope. There
 * shouldn't be a need to anyway. Thanks to add_task, test steps, even
 * though asynchronous, can be described in a nice flat way, and
 * if/for/while/... control flow can be used as in sync code, making it
 * possible to write the outline of the test case all in add_task, and delegate
 * actual processing and assertions to other functions.
 */

/* *********************************************
 * UTILS
 * *********************************************
 * General test utilities.
 * Add new tabs, open the toolbox and switch to the various panels, select
 * nodes, get node references, ...
 */

/**
 * The rule-view tests rely on a frame-script to be injected in the content test
 * page. So override the shared-head's addTab to load the frame script after the
 * tab was added.
 * FIXME: Refactor the rule-view tests to use the testActor instead of a frame
 * script, so they can run on remote targets too.
 */
var _addTab = addTab;
addTab = function (url) {
  return _addTab(url).then(tab => {
    info("Loading the helper frame script " + FRAME_SCRIPT_URL);
    let browser = tab.linkedBrowser;
    browser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);
    return tab;
  });
};

/**
 * Polls a given function waiting for it to return true.
 *
 * @param {Function} validatorFn
 *        A validator function that returns a boolean.
 *        This is called every few milliseconds to check if the result is true.
 *        When it is true, the promise resolves.
 * @param {String} name
 *        Optional name of the test. This is used to generate
 *        the success and failure messages.
 * @return a promise that resolves when the function returned true or rejects
 * if the timeout is reached
 */
function waitForSuccess(validatorFn, name = "untitled") {
  let def = defer();

  function wait(validator) {
    if (validator()) {
      ok(true, "Validator function " + name + " returned true");
      def.resolve();
    } else {
      setTimeout(() => wait(validator), 200);
    }
  }
  wait(validatorFn);

  return def.promise;
}

/**
 * Get the dataURL for the font family tooltip.
 *
 * @param {String} font
 *        The font family value.
 * @param {object} nodeFront
 *        The NodeActor that will used to retrieve the dataURL for the
 *        font family tooltip contents.
 */
var getFontFamilyDataURL = Task.async(function* (font, nodeFront) {
  let fillStyle = getThemeColor("body-color");

  let {data} = yield nodeFront.getFontFamilyDataURL(font, fillStyle);
  let dataURL = yield data.string();
  return dataURL;
});

/* *********************************************
 * RULE-VIEW
 * *********************************************
 * Rule-view related test utility functions
 * This object contains functions to get rules, get properties, ...
 */

/**
 * Simulate a color change in a given color picker tooltip, and optionally wait
 * for a given element in the page to have its style changed as a result
 *
 * @param {RuleView} ruleView
 *        The related rule view instance
 * @param {SwatchColorPickerTooltip} colorPicker
 * @param {Array} newRgba
 *        The new color to be set [r, g, b, a]
 * @param {Object} expectedChange
 *        Optional object that needs the following props:
 *          - {DOMNode} element The element in the page that will have its
 *            style changed.
 *          - {String} name The style name that will be changed
 *          - {String} value The expected style value
 * The style will be checked like so: getComputedStyle(element)[name] === value
 */
var simulateColorPickerChange = Task.async(function* (ruleView, colorPicker,
    newRgba, expectedChange) {
  let onRuleViewChanged = ruleView.once("ruleview-changed");
  info("Getting the spectrum colorpicker object");
  let spectrum = yield colorPicker.spectrum;
  info("Setting the new color");
  spectrum.rgb = newRgba;
  info("Applying the change");
  spectrum.updateUI();
  spectrum.onChange();
  info("Waiting for rule-view to update");
  yield onRuleViewChanged;

  if (expectedChange) {
    info("Waiting for the style to be applied on the page");
    yield waitForSuccess(() => {
      let {element, name, value} = expectedChange;
      return content.getComputedStyle(element)[name] === value;
    }, "Color picker change applied on the page");
  }
});

/* *********************************************
 * COMPUTED-VIEW
 * *********************************************
 * Computed-view related utility functions.
 * Allows to get properties, links, expand properties, ...
 */

/**
 * Get references to the name and value span nodes corresponding to a given
 * property name in the computed-view
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {String} name
 *        The name of the property to retrieve
 * @return an object {nameSpan, valueSpan}
 */
function getComputedViewProperty(view, name) {
  let prop;
  for (let property of view.styleDocument.querySelectorAll(".property-view")) {
    let nameSpan = property.querySelector(".property-name");
    let valueSpan = property.querySelector(".property-value");

    if (nameSpan.firstChild.textContent === name) {
      prop = {nameSpan, valueSpan};
      break;
    }
  }
  return prop;
}

/**
 * Get the text value of the property corresponding to a given name in the
 * computed-view
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {String} name
 *        The name of the property to retrieve
 * @return {String} The property value
 */
function getComputedViewPropertyValue(view, name, propertyName) {
  return getComputedViewProperty(view, name, propertyName).valueSpan.textContent;
}
