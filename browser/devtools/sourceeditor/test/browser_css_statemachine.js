/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const cssAutoCompleter  = require("devtools/sourceeditor/css-autocompleter");
const { Cc, Ci } = require("chrome");

const CSS_URI = "http://mochi.test:8888/browser/browser/devtools/sourceeditor" +
                "/test/css_statemachine_testcases.css";
const TESTS_URI = "http://mochi.test:8888/browser/browser/devtools/sourceeditor" +
                  "/test/css_statemachine_tests.json";

const source = read(CSS_URI);
const tests = eval(read(TESTS_URI));

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

let doc = null;
function test() {
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    runTests();
  }, true);
  content.location = TEST_URI;
}

function runTests() {
  let completer = new cssAutoCompleter();
  let checkState = state => {
    if (state[0] == 'null' && (!completer.state || completer.state == 'null')) {
      return true;
    } else if (state[0] == completer.state && state[0] == 'selector' &&
               state[1] == completer.selectorState &&
               state[2] == completer.completing &&
               state[3] == completer.selector) {
      return true;
    } else if (state[0] == completer.state && state[0] == 'value' &&
               state[2] == completer.completing &&
               state[3] == completer.propertyName) {
      return true;
    } else if (state[0] == completer.state &&
               state[2] == completer.completing &&
               state[0] != 'selector' && state[0] != 'value') {
      return true;
    }
    return false;
  };

  let progress = doc.getElementById("progress");
  let progressDiv = doc.querySelector("#progress > div");
  let i = 0;
  for (let test of tests) {
    progress.dataset.progress = ++i;
    progressDiv.style.width = 100*i/tests.length + "%";
    completer.resolveState(limit(source, test[0]),
                           {line: test[0][0], ch: test[0][1]});
    if (checkState(test[1])) {
      ok(true, "Test " + i + " passed. ");
    }
    else {
      ok(false, "Test " + i + " failed. Expected state : [" + test[1] + "] but" +
         " found [" + completer.state + ", " + completer.selectorState + ", " +
         completer.completing + ", " +
         (completer.propertyName || completer.selector) + "].");
      progress.classList.add("failed");
    }
  }
  gBrowser.removeCurrentTab();
  finish();
}
