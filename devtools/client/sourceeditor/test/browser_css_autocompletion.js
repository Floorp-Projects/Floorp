/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CSSCompleter = require("devtools/client/sourceeditor/css-autocompleter");
const {InspectorFront} = require("devtools/shared/fronts/inspector");

const CSS_URI = "http://mochi.test:8888/browser/devtools/client/sourceeditor" +
                "/test/css_statemachine_testcases.css";
const TESTS_URI = "http://mochi.test:8888/browser/devtools/client" +
                  "/sourceeditor/test/css_autocompletion_tests.json";

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
   "  <div id='devtools-menu' class='devtools-toolbarbutton'></div>",
   "  <div id='devtools-toolbarbutton' class='devtools-menulist'></div>",
   "  <div id='devtools-anotherone'></div>",
   "  <div id='devtools-yetagain'></div>",
   "  <div id='devtools-itjustgoeson'></div>",
   "  <div id='devtools-okstopitnow'></div>",
   "  <div class='hidden-labels-box devtools-toolbarbutton devtools-menulist'></div>",
   "  <div class='devtools-menulist'></div>",
   "  <div class='devtools-menulist'></div>",
   /* eslint-disable max-len */
   "  <tabs class='devtools-toolbarbutton'><tab></tab><tab></tab><tab></tab></tabs><tabs></tabs>",
   /* eslint-enable max-len */
   "  <button class='category-name visible'></button>",
   "  <div class='devtools-toolbarbutton' label='true'>",
   "   <hbox class='toolbarbutton-menubutton-button'></hbox></div>",
   " </body>",
   " </html>"
  ].join("\n"));

let browser;
let index = 0;
let completer = null;
let inspector;

add_task(async function test() {
  const tab = await addTab(TEST_URI);
  browser = tab.linkedBrowser;
  await runTests();
  browser = null;
  gBrowser.removeCurrentTab();
});

async function runTests() {
  const target = TargetFactory.forTab(gBrowser.selectedTab);
  await target.makeRemote();
  inspector = InspectorFront(target.client, target.form);
  const walker = await inspector.getWalker();
  completer = new CSSCompleter({walker: walker,
                                cssProperties: getClientCssProperties()});
  await checkStateAndMoveOn();
  await completer.walker.release();
  inspector.destroy();
  inspector = null;
  completer = null;
}

async function checkStateAndMoveOn() {
  if (index == tests.length) {
    return;
  }

  const [lineCh, expectedSuggestions] = tests[index];
  const [line, ch] = lineCh;

  ++index;
  await ContentTask.spawn(browser, [index, tests.length], function([idx, len]) {
    const progress = content.document.getElementById("progress");
    const progressDiv = content.document.querySelector("#progress > div");
    progress.dataset.progress = idx;
    progressDiv.style.width = 100 * idx / len + "%";
  });

  const actualSuggestions = await completer.complete(limit(source, lineCh), {line, ch});
  await checkState(expectedSuggestions, actualSuggestions);
  await checkStateAndMoveOn();
}

async function checkState(expected, actual) {
  if (expected.length != actual.length) {
    ok(false, "Number of suggestions did not match up for state " + index +
              ". Expected: " + expected.length + ", Actual: " + actual.length);
    await ContentTask.spawn(browser, null, function() {
      const progress = content.document.getElementById("progress");
      progress.classList.add("failed");
    });
    return;
  }

  for (let i = 0; i < actual.length; i++) {
    if (expected[i] != actual[i].label) {
      ok(false, "Suggestion " + i + " of state " + index + " did not match up" +
                 ". Expected: " + expected[i] + ". Actual: " + actual[i].label);
      return;
    }
  }
  ok(true, "Test " + index + " passed. ");
}
