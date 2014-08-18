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

let {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
let {CssLogic} = require("devtools/styleinspector/css-logic");

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

let dumpn = msg => dump(msg + "\n");
