/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the SourceMapService updates generated sources when source maps
 * are subsequently found. Also checks when no column is provided, and
 * when tagging an already source mapped location initially.
 */

// Empty page
const PAGE_URL = `${URL_ROOT}doc_empty-tab-01.html`;
const JS_URL = `${URL_ROOT}code_binary_search.js`;
const COFFEE_URL = `${URL_ROOT}code_binary_search.coffee`;

add_task(function* () {
  yield pushPref("devtools.debugger.new-debugger-frontend", true);

  const toolbox = yield openNewTabAndToolbox(PAGE_URL, "jsdebugger");
  const service = toolbox.sourceMapURLService;

  // Inject JS script
  let sourceSeen = waitForSourceLoad(toolbox, JS_URL);
  yield createScript(JS_URL);
  yield sourceSeen;

  let loc1 = { url: JS_URL, line: 6 };
  let newLoc1 = yield service.originalPositionFor(loc1.url, loc1.line);
  checkLoc1(loc1, newLoc1);

  let loc2 = { url: JS_URL, line: 8, column: 3 };
  let newLoc2 = yield service.originalPositionFor(loc2.url, loc2.line, loc2.column);
  checkLoc2(loc2, newLoc2);

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
  is(newLoc.sourceUrl, COFFEE_URL, "Correct url for JS:6 -> COFFEE");
}

function checkLoc2(oldLoc, newLoc) {
  is(oldLoc.line, 8, "Correct line for JS:8:3");
  is(oldLoc.column, 3, "Correct column for JS:8:3");
  is(oldLoc.url, JS_URL, "Correct url for JS:8:3");
  is(newLoc.line, 6, "Correct line for JS:8:3 -> COFFEE");
  is(newLoc.column, 10, "Correct column for JS:8:3 -> COFFEE");
  is(newLoc.sourceUrl, COFFEE_URL, "Correct url for JS:8:3 -> COFFEE");
}
