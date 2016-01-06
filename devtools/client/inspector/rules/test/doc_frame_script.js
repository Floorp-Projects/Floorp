/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// A helper frame-script for brower/devtools/styleinspector tests.
//
// Most listeners in the script expect "Test:"-namespaced messages from chrome,
// then execute code upon receiving, and immediately send back a message.
// This is so that chrome test code can execute code in content and wait for a
// response this way:
// let response = yield executeInContent(browser, "Test:MessageName", data, true);
// The response message should have the same name "Test:MessageName"
//
// Some listeners do not send a response message back.

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

var {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
var {CssLogic} = require("devtools/shared/styleinspector/css-logic");
var promise = require("promise");

/**
 * Get a value for a given property name in a css rule in a stylesheet, given
 * their indexes
 * @param {Object} data Expects a data object with the following properties
 * - {Number} styleSheetIndex
 * - {Number} ruleIndex
 * - {String} name
 * @return {String} The value, if found, null otherwise
 */
addMessageListener("Test:GetRulePropertyValue", function(msg) {
  let {name, styleSheetIndex, ruleIndex} = msg.data;
  let value = null;

  dumpn("Getting the value for property name " + name + " in sheet " +
    styleSheetIndex + " and rule " + ruleIndex);

  let sheet = content.document.styleSheets[styleSheetIndex];
  if (sheet) {
    let rule = sheet.cssRules[ruleIndex];
    if (rule) {
      value = rule.style.getPropertyValue(name);
    }
  }

  sendAsyncMessage("Test:GetRulePropertyValue", value);
});

/**
 * Get information about all the stylesheets that contain rules that apply to
 * a given node. The information contains the sheet href and whether or not the
 * sheet is a content sheet or not
 * @param {Object} objects Expects a 'target' CPOW object
 * @return {Array} A list of stylesheet info objects
 */
addMessageListener("Test:GetStyleSheetsInfoForNode", function(msg) {
  let target = msg.objects.target;
  let sheets = [];

  let domUtils = Cc["@mozilla.org/inspector/dom-utils;1"]
    .getService(Ci.inIDOMUtils);
  let domRules = domUtils.getCSSStyleRules(target);

  for (let i = 0, n = domRules.Count(); i < n; i++) {
    let sheet = domRules.GetElementAt(i).parentStyleSheet;
    sheets.push({
      href: sheet.href,
      isContentSheet: CssLogic.isContentStylesheet(sheet)
    });
  }

  sendAsyncMessage("Test:GetStyleSheetsInfoForNode", sheets);
});

/**
 * Get the property value from the computed style for an element.
 * @param {Object} data Expects a data object with the following properties
 * - {String} selector: The selector used to obtain the element.
 * - {String} pseudo: pseudo id to query, or null.
 * - {String} name: name of the property
 * @return {String} The value, if found, null otherwise
 */
addMessageListener("Test:GetComputedStylePropertyValue", function(msg) {
  let {selector, pseudo, name} = msg.data;
  let element = content.document.querySelector(selector);
  let value = content.document.defaultView.getComputedStyle(element, pseudo).getPropertyValue(name);
  sendAsyncMessage("Test:GetComputedStylePropertyValue", value);
});

/**
 * Wait the property value from the computed style for an element and
 * compare it with the expected value
 * @param {Object} data Expects a data object with the following properties
 * - {String} selector: The selector used to obtain the element.
 * - {String} pseudo: pseudo id to query, or null.
 * - {String} name: name of the property
 * - {String} expected: the expected value for property
 */
addMessageListener("Test:WaitForComputedStylePropertyValue", function(msg) {
  let {selector, pseudo, name, expected} = msg.data;
  let element = content.document.querySelector(selector);
  waitForSuccess(() => {
    let value = content.document.defaultView.getComputedStyle(element, pseudo)
                                            .getPropertyValue(name);

    return value === expected;
  }).then(() => {
    sendAsyncMessage("Test:WaitForComputedStylePropertyValue");
  })
});


var dumpn = msg => dump(msg + "\n");

/**
 * Polls a given function waiting for it to return true.
 *
 * @param {Function} validatorFn A validator function that returns a boolean.
 * This is called every few milliseconds to check if the result is true. When
 * it is true, the promise resolves.
 * @param {String} name Optional name of the test. This is used to generate
 * the success and failure messages.
 * @return a promise that resolves when the function returned true or rejects
 * if the timeout is reached
 */
function waitForSuccess(validatorFn, name="untitled") {
  let def = promise.defer();

  function wait(validatorFn) {
    if (validatorFn()) {
      def.resolve();
    } else {
      setTimeout(() => wait(validatorFn), 200);
    }
  }
  wait(validatorFn);

  return def.promise;
}
