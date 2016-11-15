/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CSSCompleter = require("devtools/client/sourceeditor/css-autocompleter");

const CSS_URI = "http://mochi.test:8888/browser/devtools/client/sourceeditor" +
                "/test/css_statemachine_testcases.css";
const TESTS_URI = "http://mochi.test:8888/browser/devtools/client" +
                  "/sourceeditor/test/css_statemachine_tests.json";

const source = read(CSS_URI);
const {tests} = JSON.parse(read(TESTS_URI));

const TEST_URI = "data:text/html;charset=UTF-8," + encodeURIComponent(
  ["<!DOCTYPE html>",
   "<html>",
   " <head>",
   "  <title>CSS State machine tests.</title>",
   "  <style type='text/css'>",
   "#progress {",
   "  width: 500px; height: 30px;",
   "  border: 1px solid black;",
   "  position: relative",
   "}",
   "#progress div {",
   "  width: 0%; height: 100%;",
   "  background: green;",
   "  position: absolute;",
   "  z-index: -1; top: 0",
   "}",
   "#progress.failed div {",
   "  background: red !important;",
   "}",
   "#progress.failed:after {",
   "  content: 'Some tests failed';",
   "  color: white",
   "}",
   "#progress:before {",
   "  content: 'Running test ' attr(data-progress) ' of " + tests.length + "';",
   "  color: white;",
   "  text-shadow: 0 0 2px darkgreen;",
   "}",
   "  </style>",
   " </head>",
   " <body>",
   "  <h2>State machine tests for CSS autocompleter.</h2><br>",
   "  <div id='progress' data-progress='0'>",
   "   <div></div>",
   "  </div>",
   " </body>",
   " </html>"
  ].join("\n"));

var doc = null;
function test() {
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab(TEST_URI);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() => {
    /* eslint-disable mozilla/no-cpows-in-tests */
    doc = content.document;
    /* eslint-enable mozilla/no-cpows-in-tests */
    runTests();
  });
}

function runTests() {
  let completer = new CSSCompleter({
    cssProperties: getClientCssProperties()
  });
  let checkState = state => {
    if (state[0] == "null" && (!completer.state || completer.state == "null")) {
      return true;
    } else if (state[0] == completer.state && state[0] == "selector" &&
               state[1] == completer.selectorState &&
               state[2] == completer.completing &&
               state[3] == completer.selector) {
      return true;
    } else if (state[0] == completer.state && state[0] == "value" &&
               state[2] == completer.completing &&
               state[3] == completer.propertyName) {
      return true;
    } else if (state[0] == completer.state &&
               state[2] == completer.completing &&
               state[0] != "selector" && state[0] != "value") {
      return true;
    }
    return false;
  };

  let progress = doc.getElementById("progress");
  let progressDiv = doc.querySelector("#progress > div");
  let i = 0;
  for (let testcase of tests) {
    progress.dataset.progress = ++i;
    progressDiv.style.width = 100 * i / tests.length + "%";
    completer.resolveState(limit(source, testcase[0]),
                           {line: testcase[0][0], ch: testcase[0][1]});
    if (checkState(testcase[1])) {
      ok(true, "Test " + i + " passed. ");
    } else {
      ok(false, "Test " + i + " failed. Expected state : [" + testcase[1] + "] " +
         "but found [" + completer.state + ", " + completer.selectorState +
         ", " + completer.completing + ", " +
         (completer.propertyName || completer.selector) + "].");
      progress.classList.add("failed");
    }
  }
  gBrowser.removeCurrentTab();
  finish();
}
