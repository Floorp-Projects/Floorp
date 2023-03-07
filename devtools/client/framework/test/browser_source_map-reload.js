/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that reloading re-reads the source maps.

"use strict";
const INITIAL_URL =
  "data:text/html,<!doctype html>html><head><meta charset='utf-8'/><title>Empty test page 1</title></head><body></body></html>";
const ORIGINAL_URL_1 = "webpack://code-reload/v1/code_reload_1.js";
const ORIGINAL_URL_2 = "webpack://code-reload/v2/code_reload_2.js";

const GENERATED_LINE = 13;
const ORIGINAL_LINE = 7;

const testServer = createVersionizedHttpTestServer("reload");

const PAGE_URL = testServer.urlFor("doc_reload.html");
const JS_URL = testServer.urlFor("code_bundle_reload.js");

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

  testServer.switchToNextVersion();

  // Reload the page. A different source file will be loaded.
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
