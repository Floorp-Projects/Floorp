/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the basic console.log()-style APIs and filtering work.

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-console.html";

var test = asyncTest(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  hud.jsterm.clearOutput();

  let outputNode = hud.outputNode;

  let methods = ["log", "info", "warn", "error", "exception", "debug"];
  for (let method of methods) {
    yield testMethod(method, hud, outputNode);
  }
});

function* testMethod(method, hud, outputNode) {
  let console = content.console;

  hud.jsterm.clearOutput();

  console[method]("foo-bar-baz");
  console[method]("baar-baz");

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foo-bar-baz",
    }, {
      text: "baar-baz",
    }],
  });

  setStringFilter("foo", hud);

  is(outputNode.querySelectorAll(".filtered-by-string").length, 1,
     "1 hidden " + method + " node via string filtering");

  hud.jsterm.clearOutput();

  // now toggle the current method off - make sure no visible message
  // TODO: move all filtering tests into a separate test file: see bug 608135

  console[method]("foo-bar-baz");
  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foo-bar-baz",
    }],
  });

  setStringFilter("", hud);
  let filter;
  switch (method) {
    case "debug":
      filter = "log";
      break;
    case "exception":
      filter = "error";
      break;
    default:
      filter = method;
      break;
  }

  hud.setFilterState(filter, false);

  is(outputNode.querySelectorAll(".filtered-by-type").length, 1,
     "1 message hidden for " + method + " (logging turned off)");

  hud.setFilterState(filter, true);

  is(outputNode.querySelectorAll(".message:not(.filtered-by-type)").length, 1,
     "1 message shown for " + method + " (logging turned on)");

  hud.jsterm.clearOutput();

  // test for multiple arguments.
  console[method]("foo", "bar");

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foo bar",
      category: CATEGORY_WEBDEV,
    }],
  });
}

function setStringFilter(value, hud) {
  hud.ui.filterBox.value = value;
  hud.ui.adjustVisibilityOnSearchStringChange();
}
