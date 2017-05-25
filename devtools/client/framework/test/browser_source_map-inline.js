/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that inline source maps work.

"use strict";

const TEST_ROOT = "http://example.com/browser/devtools/client/framework/test/";
// Empty page
const PAGE_URL = `${TEST_ROOT}doc_empty-tab-01.html`;
const JS_URL = `${TEST_ROOT}code_inline_bundle.js`;
const ORIGINAL_URL = "webpack:///code_inline_original.js";

add_task(function* () {
  yield pushPref("devtools.debugger.new-debugger-frontend", true);

  const toolbox = yield openNewTabAndToolbox(PAGE_URL, "jsdebugger");
  const service = toolbox.sourceMapURLService;

  // Inject JS script
  let sourceSeen = waitForSourceLoad(toolbox, JS_URL);
  yield createScript(JS_URL);
  yield sourceSeen;

  info(`checking original location for ${JS_URL}:84`);
  let newLoc = yield service.originalPositionFor(JS_URL, 84);

  is(newLoc.sourceUrl, ORIGINAL_URL, "check mapped URL");
  is(newLoc.line, 11, "check mapped line number");

  yield toolbox.destroy();
  gBrowser.removeCurrentTab();
  finish();
});
