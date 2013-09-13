/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the basic console.log()-style APIs and filtering work.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

let testDriver = null;
let subtestDriver = null;

function test() {
  addTab(TEST_URI);

  browser.addEventListener("DOMContentLoaded", onLoad, false);
}

function onLoad() {
  browser.removeEventListener("DOMContentLoaded", onLoad, false);

  openConsole(null, function(aHud) {
    hud = aHud;
    hudId = hud.hudId;
    outputNode = hud.outputNode;
    testDriver = testGen();
    testDriver.next();
  });
}

function testGen() {
  subtestGen("log");
  yield undefined;

  subtestGen("info");
  yield undefined;

  subtestGen("warn");
  yield undefined;

  subtestGen("error");
  yield undefined;

  subtestGen("debug"); // bug 616742
  yield undefined;

  testDriver = subtestDriver = null;
  finishTest();

  yield undefined;
}

function subtestGen(aMethod) {
  subtestDriver = testConsoleLoggingAPI(aMethod);
  subtestDriver.next();
}

function testConsoleLoggingAPI(aMethod) {
  let console = content.wrappedJSObject.console;

  hud.jsterm.clearOutput();

  setStringFilter("foo");
  console[aMethod]("foo-bar-baz");
  console[aMethod]("bar-baz");

  function nextTest() {
    subtestDriver.next();
  }

  waitForSuccess({
    name: "1 hidden " + aMethod + " node via string filtering",
    validatorFn: function()
    {
      return outputNode.querySelectorAll(".filtered-by-string").length == 1;
    },
    successFn: nextTest,
    failureFn: nextTest,
  });

  yield undefined;

  hud.jsterm.clearOutput();

  // now toggle the current method off - make sure no visible message

  // TODO: move all filtering tests into a separate test file: see bug 608135
  setStringFilter("");
  let filter = aMethod == "debug" ? "log" : aMethod;
  hud.setFilterState(filter, false);
  console[aMethod]("foo-bar-baz");

  waitForSuccess({
    name: "1 message hidden for " + aMethod + " (logging turned off)",
    validatorFn: function()
    {
      return outputNode.querySelectorAll(".filtered-by-type").length == 1;
    },
    successFn: nextTest,
    failureFn: nextTest,
  });

  yield undefined;

  hud.jsterm.clearOutput();
  hud.setFilterState(filter, true);
  console[aMethod]("foo-bar-baz");

  waitForSuccess({
    name: "1 message shown for " + aMethod + " (logging turned on)",
    validatorFn: function()
    {
      return outputNode.querySelectorAll(".message:not(.filtered-by-type)").length == 1;
    },
    successFn: nextTest,
    failureFn: nextTest,
  });

  yield undefined;

  hud.jsterm.clearOutput();
  setStringFilter("");

  // test for multiple arguments.
  console[aMethod]("foo", "bar");

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: '"foo" "bar"',
      category: CATEGORY_WEBDEV,
    }],
  }).then(nextTest);

  yield undefined;
  testDriver.next();
  yield undefined;
}

function setStringFilter(aValue) {
  hud.ui.filterBox.value = aValue;
  hud.ui.adjustVisibilityOnSearchStringChange();
}

