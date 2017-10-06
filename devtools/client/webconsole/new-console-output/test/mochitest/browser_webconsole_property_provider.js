/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the property provider, which is part of the code completion
// infrastructure.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>test the JS property provider";

function test() {
  loadTab(TEST_URI).then(testPropertyProvider);
}

function testPropertyProvider({browser}) {
  browser.removeEventListener("load", testPropertyProvider, true);
  let {JSPropertyProvider} = require("devtools/shared/webconsole/js-property-provider");

  let tmp = Cu.import("resource://gre/modules/jsdebugger.jsm", {});
  tmp.addDebuggerToGlobal(tmp);
  let dbg = new tmp.Debugger();
  let dbgWindow = dbg.addDebuggee(content);

  let completion = JSPropertyProvider(dbgWindow, null, "thisIsNotDefined");
  is(completion.matches.length, 0, "no match for 'thisIsNotDefined");

  // This is a case the PropertyProvider can't handle. Should return null.
  completion = JSPropertyProvider(dbgWindow, null, "window[1].acb");
  is(completion, null, "no match for 'window[1].acb");

  // A very advanced completion case.
  let strComplete =
    "function a() { }document;document.getElementById(window.locatio";
  completion = JSPropertyProvider(dbgWindow, null, strComplete);
  ok(completion.matches.length == 2, "two matches found");
  ok(completion.matchProp == "locatio", "matching part is 'test'");
  let matches = completion.matches;
  matches.sort();
  ok(matches[0] == "location", "the first match is 'location'");
  ok(matches[1] == "locationbar", "the second match is 'locationbar'");

  dbg.removeDebuggee(content);
  finishTest();
}
