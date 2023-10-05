/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* import-globals-from ../head.js */

/**
 * This second test will focus on v1/removed-original.js which is an original source mapped file.
 * This source is mapped to replaced-bundle.js.
 * In the first reload (v2), this original source is removed and another original file: v2/new-original.js
 * will replace the content of the removed-original.js in the replaced-bundle.js generated file.
 * And finally, in the second reload (v3) everything is removed, both original and generated source.
 *
 * Note that great care is done to ensure that new-original replaces removed-original with the
 * exact same breakable lines and columns. So that the breakpoint isn't simply removed
 * because the location is no longer breakable.
 */

"use strict";

addIntegrationTask(async function testReloadingRemovedOriginalSources(
  testServer,
  testUrl,
  { isCompressed }
) {
  info("  # Test reloading a source that is replaced and then removed");
  testServer.backToFirstVersion();

  const dbg = await initDebuggerWithAbsoluteURL(testUrl, "removed-original.js");

  info("Add initial breakpoint");
  await selectSource(dbg, "removed-original.js");
  await addBreakpoint(dbg, "removed-original.js", 4);

  // Assert the precise behavior of the breakpoint before reloading
  invokeInTab("removedOriginal");
  await waitForPaused(dbg);
  const replacedSource = findSource(dbg, "removed-original.js");
  assertPausedAtSourceAndLine(dbg, replacedSource.id, 4);
  assertTextContentOnLine(dbg, 4, 'console.log("Removed original");');
  await assertBreakpoint(dbg, 4);

  is(dbg.selectors.getBreakpointCount(), 1, "One breakpoint exists");
  is(
    dbg.client.getServerBreakpointsList().length,
    1,
    "One breakpoint exists on the server"
  );

  let breakpoint = dbg.selectors.getBreakpointsList()[0];
  is(breakpoint.location.source.url, replacedSource.url);
  is(breakpoint.location.line, 4);
  if (isCompressed) {
    is(breakpoint.generatedLocation.line, 1);
    is(breakpoint.generatedLocation.column, 992);
  } else {
    is(breakpoint.generatedLocation.line, 80);
  }
  info(
    "Assert that the breakpoint snippet is originaly set to the to-be-removed original source content"
  );
  assertBreakpointSnippet(dbg, 1, `console.log("Removed original");`);

  await resume(dbg);

  info(
    "Reload, which should remove the original file and a add a new original file which will replace its content in the  generated file"
  );
  const syncBp = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  testServer.switchToNextVersion();
  const onReloaded = reload(dbg, "new-original.js");
  await syncBp;

  // Assert the new breakpoint being created after reload
  // For now, the current behavior of the debugger is that:
  // the breakpoint is still hit based on the generated source/bundle file
  // and the UI updates itself to mention the new original file.
  await waitForPaused(dbg);
  const newSource = findSource(dbg, "new-original.js");
  assertPausedAtSourceAndLine(dbg, newSource.id, 4);
  assertTextContentOnLine(dbg, 4, 'console.log("New original");');
  await assertBreakpoint(dbg, 4);

  is(dbg.selectors.getBreakpointCount(), 1, "One breakpoint exists");
  is(
    dbg.client.getServerBreakpointsList().length,
    1,
    "One breakpoint exists on the server"
  );

  breakpoint = dbg.selectors.getBreakpointsList()[0];
  is(breakpoint.location.source.url, newSource.url);
  is(breakpoint.location.line, 4);
  if (isCompressed) {
    is(breakpoint.generatedLocation.line, 1);
    is(breakpoint.generatedLocation.column, 992);
  } else {
    is(breakpoint.generatedLocation.line, 80);
  }
  info(
    "Assert that the breakpoint snippet changed to the new original source content"
  );
  assertBreakpointSnippet(dbg, 1, `console.log("New original");`);

  await resume(dbg);
  info("Wait for reload to complete after resume");
  await onReloaded;

  info(
    "Reload a last time to remove both original and generated sources entirely"
  );
  testServer.switchToNextVersion();
  await reload(dbg);

  // Let some time for breakpoint syncing to be buggy and recreated unexpected breakpoint
  await wait(1000);
  info("Assert that sources and breakpoints are gone and we aren't paused");
  ok(
    !sourceExists(dbg, "removed-original.js"),
    "removed-original is not present"
  );
  ok(!sourceExists(dbg, "new-original.js"), "new-original is not present");
  ok(
    !sourceExists(dbg, "replaced-bundle.js"),
    "replaced-bundle is not present"
  );
  assertNotPaused(dbg);
  is(dbg.selectors.getBreakpointCount(), 0, "We no longer have any breakpoint");
  // The breakpoint for the removed source still exists, atm this difficult to fix
  // as the frontend never loads the source.
  is(
    dbg.client.getServerBreakpointsList().length,
    1,
    "One breakpoint still exists on the server"
  );
});
