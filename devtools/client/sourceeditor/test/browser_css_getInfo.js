/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CSSCompleter =
      require("devtools/client/sourceeditor/css-autocompleter");

const source = [
  ".devtools-toolbar {",
  "  -moz-appearance: none;",
  "           padding:4px 3px;border-bottom-width: 1px;",
  "  border-bottom-style: solid;",
  "}",
  "",
  "#devtools-menu.devtools-menulist,",
  ".devtools-toolbarbutton#devtools-menu {",
  "  -moz-appearance: none;",
  "  -moz-box-align: center;",
  "  min-width: 78px;",
  "  min-height: 22px;",
  "  text-shadow: 0 -1px 0 hsla(210,8%,5%,.45);",
  "  border: 1px solid hsla(210,8%,5%,.45);",
  "  border-radius: 3px;",
  "  background: linear-gradient(hsla(212,7%,57%,.35),",
  "              hsla(212,7%,57%,.1)) padding-box;",
  "  margin: 0 3px;",
  "  color: inherit;",
  "}",
  "",
  ".devtools-toolbarbutton > hbox.toolbarbutton-menubutton-button {",
  "  -moz-box-orient: horizontal;",
  "}",
  "",
  ".devtools-menulist:active,",
  "#devtools-toolbarbutton:focus {",
  "  outline: 1px dotted hsla(210,30%,85%,0.7);",
  "  outline-offset   :  -4px;",
  "}",
  "",
  ".devtools-toolbarbutton:not([label]) {",
  "  min-width: 32px;",
  "}",
  "",
  ".devtools-toolbarbutton:not([label]) > .toolbarbutton-text, .devtools-toolbar {",
  "  display: none;",
  "}",
].join("\n");

// Format of test cases :
// [
//  {line, ch}, - The caret position at which the getInfo call should be made
//  expectedState, - The expected state at the caret
//  expectedSelector, - The expected selector for the state
//  expectedProperty, - The expected property name for states value and property
//  expectedValue, - If state is value, then the expected value
// ]

/* eslint-disable max-len */
const tests = [
  [{line: 0, ch: 13}, "selector", ".devtools-toolbar"],
  [{line: 8, ch: 13}, "property", ["#devtools-menu.devtools-menulist",
                                   ".devtools-toolbarbutton#devtools-menu "], "-moz-appearance"],
  [{line: 28, ch: 25}, "value", [".devtools-menulist:active",
                                 "#devtools-toolbarbutton:focus "], "outline-offset", "-4px"],
  [{line: 4, ch: 1}, "null"],
  [{line: 5, ch: 0}, "null"],
  [{line: 31, ch: 13}, "selector", ".devtools-toolbarbutton:not([label])"],
  [{line: 35, ch: 23}, "selector", ".devtools-toolbarbutton:not([label]) > .toolbarbutton-text"],
  [{line: 35, ch: 70}, "selector", ".devtools-toolbar"],
  [{line: 27, ch: 14}, "value", [".devtools-menulist:active",
                                 "#devtools-toolbarbutton:focus "], "outline", "1px dotted hsla(210,30%,85%,0.7)"],
  [{line: 16, ch: 16}, "value", ["#devtools-menu.devtools-menulist",
                                 ".devtools-toolbarbutton#devtools-menu "], "background",
   "linear-gradient(hsla(212,7%,57%,.35),\n              hsla(212,7%,57%,.1)) padding-box"],
  [{line: 16, ch: 3}, "value", ["#devtools-menu.devtools-menulist",
                                ".devtools-toolbarbutton#devtools-menu "], "background",
   "linear-gradient(hsla(212,7%,57%,.35),\n              hsla(212,7%,57%,.1)) padding-box"],
  [{line: 15, ch: 25}, "value", ["#devtools-menu.devtools-menulist",
                                 ".devtools-toolbarbutton#devtools-menu "], "background",
   "linear-gradient(hsla(212,7%,57%,.35),\n              hsla(212,7%,57%,.1)) padding-box"],
];
/* eslint-enable max-len */

const TEST_URI = "data:text/html;charset=UTF-8," + encodeURIComponent(
  ["<!DOCTYPE html>",
   "<html>",
   " <head>",
   "  <title>CSS contextual information tests.</title>",
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

add_task(async function test() {
  const tab = await addTab(TEST_URI);
  const browser = tab.linkedBrowser;

  const completer = new CSSCompleter({
    cssProperties: getClientCssProperties()
  });
  const matches = (arr, toCheck) => !arr.some((x, i) => x != toCheck[i]);
  const checkState = (expected, actual) => {
    if (expected[0] == "null" && actual == null) {
      return true;
    } else if (expected[0] == actual.state && expected[0] == "selector" &&
               expected[1] == actual.selector) {
      return true;
    } else if (expected[0] == actual.state && expected[0] == "property" &&
               matches(expected[1], actual.selectors) &&
               expected[2] == actual.propertyName) {
      return true;
    } else if (expected[0] == actual.state && expected[0] == "value" &&
               matches(expected[1], actual.selectors) &&
               expected[2] == actual.propertyName &&
               expected[3] == actual.value) {
      return true;
    }
    return false;
  };

  let i = 0;
  for (const expected of tests) {
    ++i;
    const caret = expected.splice(0, 1)[0];
    await ContentTask.spawn(browser, [i, tests.length], function([idx, len]) {
      const progress = content.document.getElementById("progress");
      const progressDiv = content.document.querySelector("#progress > div");
      progress.dataset.progress = idx;
      progressDiv.style.width = 100 * idx / len + "%";
    });
    const actual = completer.getInfoAt(source, caret);
    if (checkState(expected, actual)) {
      ok(true, "Test " + i + " passed. ");
    } else {
      ok(false, "Test " + i + " failed. Expected state : [" + expected + "] " +
         "but found [" + actual.state + ", " +
         (actual.selector || actual.selectors) + ", " +
         actual.propertyName + ", " + actual.value + "].");
      await ContentTask.spawn(browser, null, function() {
        const progress = content.document.getElementById("progress");
        progress.classList.add("failed");
      });
    }
  }
  gBrowser.removeCurrentTab();
});
