/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that inline source maps work.

"use strict";

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm");
PromiseTestUtils.whitelistRejectionsGlobally(/this\.worker is null/);
PromiseTestUtils.whitelistRejectionsGlobally(/Component not initialized/);

const TEST_ROOT = "http://example.com/browser/devtools/client/framework/test/";
// Empty page
const PAGE_URL = `${TEST_ROOT}doc_empty-tab-01.html`;
const JS_URL = `${TEST_ROOT}code_inline_bundle.js`;
const ORIGINAL_URL = "webpack:///code_inline_original.js";

add_task(async function() {
  const toolbox = await openNewTabAndToolbox(PAGE_URL, "jsdebugger");
  const service = toolbox.sourceMapURLService;

  // Inject JS script
  const sourceSeen = waitForSourceLoad(toolbox, JS_URL);
  await createScript(JS_URL);
  await sourceSeen;

  info(`checking original location for ${JS_URL}:84`);
  const newLoc = await service.originalPositionFor(JS_URL, 84);

  is(newLoc.sourceUrl, ORIGINAL_URL, "check mapped URL");
  is(newLoc.line, 11, "check mapped line number");

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
  finish();
});
