/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* globals addMessageListener, sendAsyncMessage */

"use strict";

// A helper frame-script for browser/devtools/styleinspector tests.
//
// Most listeners in the script expect "Test:"-namespaced messages from chrome,
// then execute code upon receiving, and immediately send back a message.
// This is so that chrome test code can execute code in content and wait for a
// response this way:
// let response = yield executeInContent(browser, "Test:msgName", data, true);
// The response message should have the same name "Test:msgName"
//
// Some listeners do not send a response message back.

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
  const {name, styleSheetIndex, ruleIndex} = msg.data;
  let value = null;

  dumpn("Getting the value for property name " + name + " in sheet " +
    styleSheetIndex + " and rule " + ruleIndex);

  const sheet = content.document.styleSheets[styleSheetIndex];
  if (sheet) {
    const rule = sheet.cssRules[ruleIndex];
    if (rule) {
      value = rule.style.getPropertyValue(name);
    }
  }

  sendAsyncMessage("Test:GetRulePropertyValue", value);
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
  const {selector, pseudo, name} = msg.data;
  const element = content.document.querySelector(selector);
  const value = content.document.defaultView.getComputedStyle(element, pseudo)
                                          .getPropertyValue(name);
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
  const {selector, pseudo, name, expected} = msg.data;
  const element = content.document.querySelector(selector);
  waitForSuccess(() => {
    const value = content.document.defaultView.getComputedStyle(element, pseudo)
                                            .getPropertyValue(name);

    return value === expected;
  }).then(() => {
    sendAsyncMessage("Test:WaitForComputedStylePropertyValue");
  });
});

var dumpn = msg => dump(msg + "\n");

/**
 * Polls a given function waiting for it to return true.
 *
 * @param {Function} validatorFn A validator function that returns a boolean.
 * This is called every few milliseconds to check if the result is true. When
 * it is true, the promise resolves.
 * @return a promise that resolves when the function returned true or rejects
 * if the timeout is reached
 */
function waitForSuccess(validatorFn) {
  return new Promise(resolve => {
    function wait(fn) {
      if (fn()) {
        resolve();
      } else {
        setTimeout(() => wait(fn), 200);
      }
    }
    wait(validatorFn);
  });
}
