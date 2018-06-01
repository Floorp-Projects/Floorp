/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that an absolute sourceRoot works.

"use strict";

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = scopedCuImport("resource://testing-common/PromiseTestUtils.jsm");
PromiseTestUtils.whitelistRejectionsGlobally(/this\.worker is null/);

// Empty page
const PAGE_URL = `${URL_ROOT}doc_empty-tab-01.html`;
const JS_URL = `${URL_ROOT}code_binary_search_absolute.js`;
const ORIGINAL_URL = `${URL_ROOT}code_binary_search.coffee`;

add_task(async function() {
  await pushPref("devtools.debugger.new-debugger-frontend", true);

  const toolbox = await openNewTabAndToolbox(PAGE_URL, "jsdebugger");
  const service = toolbox.sourceMapURLService;

  // Inject JS script
  const sourceSeen = waitForSourceLoad(toolbox, JS_URL);
  await createScript(JS_URL);
  await sourceSeen;

  info(`checking original location for ${JS_URL}:6`);
  const newLoc = await service.originalPositionFor(JS_URL, 6, 4);

  is(newLoc.sourceUrl, ORIGINAL_URL, "check mapped URL");
  is(newLoc.line, 4, "check mapped line number");
});
