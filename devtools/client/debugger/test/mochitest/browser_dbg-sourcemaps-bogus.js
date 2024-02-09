/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that errors while loading sourcemap does not break debugging.

"use strict";

requestLongerTimeout(2);

add_task(async function () {
  // NOTE: the CORS call makes the test run times inconsistent

  // - non-existant-map.js has a reference to source map file which doesn't exists.
  //   There is no particular warning and no original file is displayed, only the generated file.
  // - map-with-failed-original-request.js has a reference to a valid source map file,
  //   but the map doesn't inline source content and refers to a URL which fails loading.
  //   (this file is based on dom-mutation.js and related map)
  const dbg = await initDebugger(
    "doc-sourcemap-bogus.html",
    "non-existant-map.js",
    "map-with-failed-original-request.js",
    "map-with-failed-original-request.original.js",
    "invalid-json-map.js"
  );
  // Make sure there is only the expected sources and we miss some original sources.
  is(dbg.selectors.getSourceCount(), 4, "Only 4 source exists");

  await selectSource(dbg, "non-existant-map.js");
  is(
    findFooterNotificationMessage(dbg),
    "Source Map Error: request failed with status 404",
    "There is a warning about the missing source map file"
  );

  // We should still be able to set breakpoints and pause in the
  // generated source.
  await addBreakpoint(dbg, "non-existant-map.js", 4);
  invokeInTab("runCode");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "non-existant-map.js").id,
    4
  );
  await resume(dbg);

  // Test a Source Map with invalid JSON
  await selectSource(dbg, "invalid-json-map.js");
  is(
    findFooterNotificationMessage(dbg),
    "Source Map Error: JSON.parse: expected property name or '}' at line 2 column 3 of the JSON data",
    "There is a warning about the missing source map file"
  );

  // Test a Source Map with missing original text content
  await selectSource(dbg, "map-with-failed-original-request.js");
  ok(
    !findElement(dbg, "editorNotificationFooter"),
    "The source-map is valid enough to not spawn a warning message"
  );
  await addBreakpoint(dbg, "map-with-failed-original-request.js", 7);
  invokeInTab("changeStyleAttribute");
  await waitForPaused(dbg);

  // As the original file can't be loaded, the generated source is automatically selected
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "map-with-failed-original-request.js").id,
    7
  );

  // The original file is visible in the source tree and can be selected,
  // but its content can't be displayed
  await selectSource(dbg, "map-with-failed-original-request.original.js");
  const notificationMessage = DEBUGGER_L10N.getFormatStr(
    "editorNotificationFooter.noOriginalScopes",
    DEBUGGER_L10N.getStr("scopes.showOriginalScopes")
  );
  is(
    findFooterNotificationMessage(dbg),
    notificationMessage,
    "There is no warning about source-map but rather one about original scopes"
  );
  is(
    getCM(dbg).getValue(),
    `Error while fetching an original source: request failed with status 404\nSource URL: ${EXAMPLE_URL}map-with-failed-original-request.original.js`
  );

  await resume(dbg);
});
