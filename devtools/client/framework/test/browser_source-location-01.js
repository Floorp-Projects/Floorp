/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejections should be fixed.
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("[object Object]");
thisTestLeaksUncaughtRejectionsAndShouldBeFixed(
  "TypeError: this.transport is null");

/**
 * Tests the SourceMapController updates generated sources when source maps
 * are subsequently found. Also checks when no column is provided, and
 * when tagging an already source mapped location initially.
 */

const DEBUGGER_ROOT = "http://example.com/browser/devtools/client/debugger/test/mochitest/";
// Empty page
const PAGE_URL = `${DEBUGGER_ROOT}doc_empty-tab-01.html`;
const JS_URL = `${DEBUGGER_ROOT}code_binary_search.js`;
const COFFEE_URL = `${DEBUGGER_ROOT}code_binary_search.coffee`;
const { SourceLocationController } = require("devtools/client/framework/source-location");

add_task(function* () {
  let toolbox = yield openNewTabAndToolbox(PAGE_URL, "jsdebugger");

  let controller = new SourceLocationController(toolbox.target);

  let aggregator = [];

  function onUpdate(oldLoc, newLoc) {
    if (oldLoc.line === 6) {
      checkLoc1(oldLoc, newLoc);
    } else if (oldLoc.line === 8) {
      checkLoc2(oldLoc, newLoc);
    } else if (oldLoc.line === 2) {
      checkLoc3(oldLoc, newLoc);
    } else {
      throw new Error(`Unexpected location update: ${JSON.stringify(oldLoc)}`);
    }
    aggregator.push(newLoc);
  }

  let loc1 = { url: JS_URL, line: 6 };
  let loc2 = { url: JS_URL, line: 8, column: 3 };
  let loc3 = { url: COFFEE_URL, line: 2, column: 0 };

  controller.bindLocation(loc1, onUpdate);
  controller.bindLocation(loc2, onUpdate);
  controller.bindLocation(loc3, onUpdate);

  // Inject JS script
  yield createScript(JS_URL);

  yield waitUntil(() => aggregator.length === 3);

  ok(aggregator.find(i => i.url === COFFEE_URL && i.line === 4), "found first updated location");
  ok(aggregator.find(i => i.url === COFFEE_URL && i.line === 6), "found second updated location");
  ok(aggregator.find(i => i.url === COFFEE_URL && i.line === 2), "found third updated location");

  yield toolbox.destroy();
  gBrowser.removeCurrentTab();
  finish();
});

function checkLoc1(oldLoc, newLoc) {
  is(oldLoc.line, 6, "Correct line for JS:6");
  is(oldLoc.column, null, "Correct column for JS:6");
  is(oldLoc.url, JS_URL, "Correct url for JS:6");
  is(newLoc.line, 4, "Correct line for JS:6 -> COFFEE");
  is(newLoc.column, 2, "Correct column for JS:6 -> COFFEE -- handles falsy column entries");
  is(newLoc.url, COFFEE_URL, "Correct url for JS:6 -> COFFEE");
}

function checkLoc2(oldLoc, newLoc) {
  is(oldLoc.line, 8, "Correct line for JS:8:3");
  is(oldLoc.column, 3, "Correct column for JS:8:3");
  is(oldLoc.url, JS_URL, "Correct url for JS:8:3");
  is(newLoc.line, 6, "Correct line for JS:8:3 -> COFFEE");
  is(newLoc.column, 10, "Correct column for JS:8:3 -> COFFEE");
  is(newLoc.url, COFFEE_URL, "Correct url for JS:8:3 -> COFFEE");
}

function checkLoc3(oldLoc, newLoc) {
  is(oldLoc.line, 2, "Correct line for COFFEE:2:0");
  is(oldLoc.column, 0, "Correct column for COFFEE:2:0");
  is(oldLoc.url, COFFEE_URL, "Correct url for COFFEE:2:0");
  is(newLoc.line, 2, "Correct line for COFFEE:2:0 -> COFFEE");
  is(newLoc.column, 0, "Correct column for COFFEE:2:0 -> COFFEE");
  is(newLoc.url, COFFEE_URL, "Correct url for COFFEE:2:0 -> COFFEE");
}

function createScript(url) {
  info(`Creating script: ${url}`);
  let mm = getFrameScript();
  let command = `
    let script = document.createElement("script");
    script.setAttribute("src", "${url}");
    document.body.appendChild(script);
    null;
  `;
  return evalInDebuggee(mm, command);
}
