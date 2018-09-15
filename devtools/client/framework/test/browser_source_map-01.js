/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the SourceMapService updates generated sources when source maps
 * are subsequently found. Also checks when no column is provided, and
 * when tagging an already source mapped location initially.
 */

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = scopedCuImport("resource://testing-common/PromiseTestUtils.jsm");
PromiseTestUtils.whitelistRejectionsGlobally(/this\.worker is null/);
PromiseTestUtils.whitelistRejectionsGlobally(/Component not initialized/);

// Empty page
const PAGE_URL = `${URL_ROOT}doc_empty-tab-01.html`;
const JS_URL = `${URL_ROOT}code_binary_search.js`;
const COFFEE_URL = `${URL_ROOT}code_binary_search.coffee`;

add_task(async function() {
  await pushPref("devtools.debugger.new-debugger-frontend", true);

  const toolbox = await openNewTabAndToolbox(PAGE_URL, "jsdebugger");
  const service = toolbox.sourceMapURLService;

  // Inject JS script
  const sourceSeen = waitForSourceLoad(toolbox, JS_URL);
  await createScript(JS_URL);
  await sourceSeen;

  const loc1 = { url: JS_URL, line: 6 };
  const newLoc1 = await service.originalPositionFor(loc1.url, loc1.line, 4);
  checkLoc1(loc1, newLoc1);

  const loc2 = { url: JS_URL, line: 8, column: 3 };
  const newLoc2 = await service.originalPositionFor(loc2.url, loc2.line, loc2.column);
  checkLoc2(loc2, newLoc2);

  await toolbox.destroy();
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
