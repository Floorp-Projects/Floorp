/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CSSCompleter = require("devtools/client/shared/sourceeditor/css-autocompleter");

const CSS_URI =
  "http://mochi.test:8888/browser/devtools/client/shared/sourceeditor" +
  "/test/css_statemachine_testcases.css";
const TESTS_URI =
  "http://mochi.test:8888/browser/devtools/client" +
  "/shared/sourceeditor/test/css_statemachine_tests.json";

const source = read(CSS_URI);
const { tests } = JSON.parse(read(TESTS_URI));

const TEST_URI =
  "data:text/html;charset=UTF-8," +
  encodeURIComponent(
    [
      "<!DOCTYPE html>",
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
      "  content: 'Running test ' attr(data-progress) ' of " +
        tests.length +
        "';",
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
      " </html>",
    ].join("\n")
  );

add_task(async function test() {
  const tab = await addTab(TEST_URI);
  const browser = tab.linkedBrowser;

  const completer = new CSSCompleter({
    cssProperties: getClientCssProperties(),
  });
  const checkState = state => {
    if (state[0] == "null" && (!completer.state || completer.state == "null")) {
      return true;
    } else if (
      state[0] == completer.state &&
      state[0] == "selector" &&
      state[1] == completer.selectorState &&
      state[2] == completer.completing &&
      state[3] == completer.selector
    ) {
      return true;
    } else if (
      state[0] == completer.state &&
      state[0] == "value" &&
      state[2] == completer.completing &&
      state[3] == completer.propertyName
    ) {
      return true;
    } else if (
      state[0] == completer.state &&
      state[2] == completer.completing &&
      state[0] != "selector" &&
      state[0] != "value"
    ) {
      return true;
    }
    return false;
  };

  let i = 0;
  for (const testcase of tests) {
    ++i;
    await SpecialPowers.spawn(browser, [[i, tests.length]], function([
      idx,
      len,
    ]) {
      const progress = content.document.getElementById("progress");
      const progressDiv = content.document.querySelector("#progress > div");
      progress.dataset.progress = idx;
      progressDiv.style.width = (100 * idx) / len + "%";
    });
    completer.resolveState(limit(source, testcase[0]), {
      line: testcase[0][0],
      ch: testcase[0][1],
    });
    if (checkState(testcase[1])) {
      ok(true, "Test " + i + " passed. ");
    } else {
      ok(
        false,
        "Test " +
          i +
          " failed. Expected state : [" +
          testcase[1] +
          "] " +
          "but found [" +
          completer.state +
          ", " +
          completer.selectorState +
          ", " +
          completer.completing +
          ", " +
          (completer.propertyName || completer.selector) +
          "]."
      );
      await SpecialPowers.spawn(browser, [], function() {
        const progress = content.document.getElementById("progress");
        progress.classList.add("failed");
      });
    }
  }
  gBrowser.removeCurrentTab();
});
