/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* import-globals-from ../head.js */

/**
 * This first test will focus on original-with-no-update.js file where the bundle-with-another-original.js file
 * content changes because of another-original.js
 */

"use strict";

addIntegrationTask(async function testReloadingChangedGeneratedSource(
  testServer,
  testUrl,
  { isCompressed }
) {
  info(
    "  # Test reload an original source whose generated source content changes"
  );
  testServer.backToFirstVersion();
  const dbg = await initDebuggerWithAbsoluteURL(
    testUrl,
    "original-with-no-update.js"
  );

  info("Add initial breakpoint");
  await selectSource(dbg, "original-with-no-update.js");
  await addBreakpoint(dbg, "original-with-no-update.js", 6);

  info("Check that only one breakpoint is set");
  is(dbg.selectors.getBreakpointCount(), 1, "Only one breakpoint exists");

  info("Check that the breakpoint location info is correct");
  let breakpoint = dbg.selectors.getBreakpointsList(dbg)[0];
  is(breakpoint.location.line, 6);
  if (isCompressed) {
    is(breakpoint.generatedLocation.line, 1);
    is(breakpoint.generatedLocation.column, 1062);
  } else {
    is(breakpoint.generatedLocation.line, 82);
  }

  const expectedOriginalFileContentOnBreakpointLine = "await baabar();";
  const expectedGeneratedFileContentOnBreakpointLine = "await baabar();";

  info("Check that the breakpoint is displayed on the correct line in the ui");
  await assertBreakpoint(dbg, 6);

  info("Check that breakpoint is on the first line within the function `foo`");
  assertTextContentOnLine(dbg, 6, expectedOriginalFileContentOnBreakpointLine);

  info(
    "Check that the breakpoint is displayed in correct location in bundle-with-another-original.js (generated source)"
  );
  await selectSource(dbg, "bundle-with-another-original.js");
  if (isCompressed) {
    await assertBreakpoint(dbg, 1);
  } else {
    await assertBreakpoint(dbg, 82);
    assertTextContentOnLine(
      dbg,
      82,
      expectedGeneratedFileContentOnBreakpointLine
    );
  }
  info(
    "Check that the source code snipper shown in the breakpoint panel is correct"
  );
  assertBreakpointSnippet(
    dbg,
    1,
    isCompressed
      ? `baabar(),console.log("YO")},foobar()}]);`
      : `await baabar();`
  );

  await closeTab(dbg, "bundle-with-another-original.js");

  // This reload changes the content of the generated file
  // which will cause the location of the breakpoint to change
  info("Reload with a new version of the file");
  const waitUntilNewBreakpointIsSet = waitForDispatch(
    dbg.store,
    "SET_BREAKPOINT"
  );
  testServer.switchToNextVersion();
  const onReloaded = reload(
    dbg,
    "bundle-with-another-original.js",
    "original-with-no-update.js"
  );
  await waitUntilNewBreakpointIsSet;

  if (!isCompressed) {
    await waitForPaused(dbg);

    // This is a bug where the server does not recieve updates to breakpoints
    // early, therefore is pauses at the old position, where no breakpoint is
    // displayed in the UI
    info("Assert that the breakpoint paused in the other original file");
    assertPausedAtSourceAndLine(
      dbg,
      findSource(dbg, "another-original.js").id,
      5
    );
    await assertNoBreakpoint(dbg, 5);
    assertTextContentOnLine(dbg, 5, "funcC();");

    info("Switch to generated source and assert that the location is correct");
    await dbg.actions.jumpToMappedSelectedLocation();
    assertPausedAtSourceAndLine(
      dbg,
      findSource(dbg, "bundle-with-another-original.js").id,
      82
    );
    await assertNoBreakpoint(dbg, 82);
    assertTextContentOnLine(dbg, 82, "funcC();");

    info("Switch back to original location before resuming");
    await dbg.actions.jumpToMappedSelectedLocation();
    await resume(dbg);
    await waitForPaused(dbg);

    info(
      "Check that the breakpoint is displayed and paused on the correct line"
    );
    assertPausedAtSourceAndLine(
      dbg,
      findSource(dbg, "original-with-no-update.js").id,
      6
    );
  } else {
    await onReloaded;
    // Assert that it does not pause in commpressed files
    assertNotPaused(dbg);
  }
  await assertBreakpoint(dbg, 6);

  info(
    "Check that though the breakpoint has moved, it is still on the first line within the function `foo`"
  );
  assertTextContentOnLine(dbg, 6, expectedOriginalFileContentOnBreakpointLine);

  info(
    "Check that the breakpoint is displayed in correct location in bundle-with-another-original.js (generated source)"
  );
  await selectSource(dbg, "bundle-with-another-original.js");
  // This scrolls the line into view so the content
  // on the line is rendered and avaliable for dom querying.
  getCM(dbg).scrollIntoView({ line: 103, ch: 0 });

  if (isCompressed) {
    await assertBreakpoint(dbg, 1);
  } else {
    assertPausedAtSourceAndLine(
      dbg,
      findSource(dbg, "bundle-with-another-original.js").id,
      103
    );
    await assertBreakpoint(dbg, 103);
    assertTextContentOnLine(
      dbg,
      103,
      expectedGeneratedFileContentOnBreakpointLine
    );
  }

  info("Check that only one breakpoint is still set");
  is(dbg.selectors.getBreakpointCount(), 1, "Only one breakpoint exists");

  info("Check that the original location has changed");
  breakpoint = dbg.selectors.getBreakpointsList(dbg)[0];
  is(breakpoint.location.line, 6);
  if (isCompressed) {
    is(breakpoint.generatedLocation.line, 1);
    is(breakpoint.generatedLocation.column, 1132);
  } else {
    is(breakpoint.generatedLocation.line, 103);
  }

  info("Check that the breakpoint snippet is still the same");
  assertBreakpointSnippet(
    dbg,
    1,
    isCompressed
      ? `baabar(),console.log("YO")},foobar()}]);`
      : `await baabar();`
  );

  if (!isCompressed) {
    await resume(dbg);
    info("Wait for reload to complete after resume");
    await onReloaded;
  }
  await closeTab(dbg, "bundle-with-another-original.js");
});
