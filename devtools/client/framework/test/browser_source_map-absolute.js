/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that an absolute sourceRoot works.

"use strict";

// Empty page
const PAGE_URL = `${URL_ROOT}doc_empty-tab-01.html`;
const JS_URL = `${URL_ROOT}code_binary_search_absolute.js`;
const ORIGINAL_URL = `${URL_ROOT}code_binary_search.coffee`;

add_task(function* () {
  yield pushPref("devtools.debugger.new-debugger-frontend", true);

  const toolbox = yield openNewTabAndToolbox(PAGE_URL, "jsdebugger");
  const service = toolbox.sourceMapURLService;

  // Inject JS script
  let sourceSeen = waitForSourceLoad(toolbox, JS_URL);
  yield createScript(JS_URL);
  yield sourceSeen;

  info(`checking original location for ${JS_URL}:6`);
  let newLoc = yield service.originalPositionFor(JS_URL, 6);

  is(newLoc.sourceUrl, ORIGINAL_URL, "check mapped URL");
  is(newLoc.line, 4, "check mapped line number");
});
