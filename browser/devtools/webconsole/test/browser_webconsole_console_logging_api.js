/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the basic console.log()-style APIs and filtering work.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

let hud, outputNode;

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    Task.spawn(runner);
  }, true);

  function* runner() {
    hud = yield openConsole();
    outputNode = hud.outputNode;

    let methods = ["log", "info", "warn", "error", "exception", "debug"];
    for (let method of methods) {
      yield testMethod(method);
    }

    executeSoon(finishTest);
  }
}

function* testMethod(aMethod) {
  let console = content.console;

  hud.jsterm.clearOutput();

  console[aMethod]("foo-bar-baz");
  console[aMethod]("baar-baz");

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foo-bar-baz",
    }, {
      text: "baar-baz",
    }],
  });

  setStringFilter("foo");

  is(outputNode.querySelectorAll(".filtered-by-string").length, 1,
     "1 hidden " + aMethod + " node via string filtering")

  hud.jsterm.clearOutput();

  // now toggle the current method off - make sure no visible message
  // TODO: move all filtering tests into a separate test file: see bug 608135

  console[aMethod]("foo-bar-baz");
  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foo-bar-baz",
    }],
  });

  setStringFilter("");
  let filter;
  switch(aMethod) {
    case "debug":
      filter = "log";
      break;
    case "exception":
      filter = "error";
      break;
    default:
      filter = aMethod;
      break;
  }

  hud.setFilterState(filter, false);

  is(outputNode.querySelectorAll(".filtered-by-type").length, 1,
     "1 message hidden for " + aMethod + " (logging turned off)")

  hud.setFilterState(filter, true);

  is(outputNode.querySelectorAll(".message:not(.filtered-by-type)").length, 1,
     "1 message shown for " + aMethod + " (logging turned on)")

  hud.jsterm.clearOutput();

  // test for multiple arguments.
  console[aMethod]("foo", "bar");

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: '"foo" "bar"',
      category: CATEGORY_WEBDEV,
    }],
  })
}

function setStringFilter(aValue) {
  hud.ui.filterBox.value = aValue;
  hud.ui.adjustVisibilityOnSearchStringChange();
}

