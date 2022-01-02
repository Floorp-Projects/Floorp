/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that reloading re-reads the source maps.

"use strict";

const INITIAL_URL = URL_ROOT_SSL + "doc_empty-tab-01.html";
const PAGE_URL = URL_ROOT_SSL + "doc_reload.html";
const JS_URL = URL_ROOT_SSL + "sjs_code_reload.sjs";

const ORIGINAL_URL_1 = "webpack:///code_reload_1.js";
const ORIGINAL_URL_2 = "webpack:///code_reload_2.js";

const GENERATED_LINE = 86;
const ORIGINAL_LINE = 13;

add_task(async function() {
  // Start with the empty page, then navigate, so that we can properly
  // listen for new sources arriving.
  const toolbox = await openNewTabAndToolbox(INITIAL_URL, "webconsole");
  const service = toolbox.sourceMapURLService;

  let sourceSeen = waitForSourceLoad(toolbox, JS_URL);
  await navigateTo(PAGE_URL);
  await sourceSeen;

  info(`checking original location for ${JS_URL}:${GENERATED_LINE}`);
  let newLoc = await new Promise(r =>
    service.subscribeByURL(JS_URL, GENERATED_LINE, undefined, r)
  );
  is(newLoc.url, ORIGINAL_URL_1, "check mapped URL");
  is(newLoc.line, ORIGINAL_LINE, "check mapped line number");

  // Reload the page.  The sjs ensures that a different source file
  // will be loaded.
  sourceSeen = waitForSourceLoad(toolbox, JS_URL);
  await reloadBrowser();
  await sourceSeen;

  info(
    `checking post-reload original location for ${JS_URL}:${GENERATED_LINE}`
  );
  newLoc = await new Promise(r =>
    service.subscribeByURL(JS_URL, GENERATED_LINE, undefined, r)
  );
  is(newLoc.url, ORIGINAL_URL_2, "check post-reload mapped URL");
  is(newLoc.line, ORIGINAL_LINE, "check post-reload mapped line number");

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
});
