/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* import-globals-from ../head.js */

/**
 * This first test will focus on original.js file whose content changes
 * which affects the related generated file: bundle.js
 *
 * In the first reload, v2/original.js will only change with new lines being added
 * before the line where we set a breakpoint. So that the breakpoint, if not
 * automatically shifted, will now be against an empty line.
 *
 * In the second reload, v3/original.js will be trimmed, so that the line
 * where we set a breakpoint against, has been removed.
 */

"use strict";

addIntegrationTask(async function testReloadingStableOriginalSource(
  testServer,
  testUrl,
  { isCompressed }
) {
  info("  # Test reload a stable source whose content changes");
  const dbg = await initDebuggerWithAbsoluteURL(testUrl, "original.js");

  info("Add initial breakpoint");
  await selectSource(dbg, "original.js");
  await addBreakpoint(dbg, "original.js", 6);

  info("Check that only one breakpoint is set");
  is(dbg.selectors.getBreakpointCount(), 1, "Only one breakpoint exists");
  is(
    dbg.client.getServerBreakpointsList().length,
    1,
    "One breakpoint exists on the server"
  );

  info("Check that the breakpoint location info is correct");
  let breakpoint = dbg.selectors.getBreakpointsList(dbg)[0];
  is(breakpoint.location.line, 6);
  if (isCompressed) {
    is(breakpoint.generatedLocation.line, 1);
    is(breakpoint.generatedLocation.column, 1056);
  } else {
    is(breakpoint.generatedLocation.line, 82);
  }

  const expectedOriginalFileContentOnBreakpointLine = "await bar();";
  const expectedGeneratedFileContentOnBreakpointLine = "await bar();";

  info("Check that the breakpoint is displayed on the correct line in the ui");
  await assertBreakpoint(dbg, 6);

  info("Check that breakpoint is on the first line within the function `foo`");
  assertTextContentOnLine(dbg, 6, expectedOriginalFileContentOnBreakpointLine);

  info(
    "Check that the breakpoint is displayed in correct location in bundle.js (generated source)"
  );
  await selectSource(dbg, "bundle.js");
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

  await closeTab(dbg, "bundle.js");

  info("Reload with a new version of the file");
  const waitUntilNewBreakpointIsSet = waitForDispatch(
    dbg.store,
    "SET_BREAKPOINT"
  );
  testServer.switchToNextVersion();
  await reload(dbg, "bundle.js", "original.js");
  await waitUntilNewBreakpointIsSet;

  info("Check that only one breakpoint is set");
  is(dbg.selectors.getBreakpointCount(), 1, "Only one breakpoint exists");
  is(
    dbg.client.getServerBreakpointsList().length,
    1,
    "One breakpoint exists on the server"
  );

  info("Check that the original location has changed");
  breakpoint = dbg.selectors.getBreakpointsList(dbg)[0];
  is(breakpoint.location.line, 9);
  if (isCompressed) {
    is(breakpoint.generatedLocation.line, 1);
    is(breakpoint.generatedLocation.column, 1056);
  } else {
    is(breakpoint.generatedLocation.line, 82);
  }

  info("Invoke `foo` to trigger breakpoint");
  invokeInTab("foo");
  await waitForPaused(dbg);

  info("Check that the breakpoint is displayed and paused on the correct line");
  const originalSource = findSource(dbg, "original.js");
  await assertPausedAtSourceAndLine(dbg, originalSource.id, 9);
  await assertBreakpoint(dbg, 9);

  info(
    "Check that though the breakpoint has moved, it is still on the first line within the function `foo`"
  );
  assertTextContentOnLine(dbg, 9, expectedOriginalFileContentOnBreakpointLine);

  info(
    "Check that the breakpoint is displayed in correct location in bundle.js (generated source)"
  );
  await selectSource(dbg, "bundle.js");
  // This scrolls the line into view so the content
  // on the line is rendered and avaliable for dom querying.
  getCM(dbg).scrollIntoView({ line: 82, ch: 0 });

  const generatedSource = findSource(dbg, "bundle.js");
  if (isCompressed) {
    await assertBreakpoint(dbg, 1);
    await assertPausedAtSourceAndLine(dbg, generatedSource.id, 1, 1056);
  } else {
    await assertBreakpoint(dbg, 82);
    await assertPausedAtSourceAndLine(dbg, generatedSource.id, 82);
  }
  if (!isCompressed) {
    assertTextContentOnLine(
      dbg,
      82,
      expectedGeneratedFileContentOnBreakpointLine
    );
  }

  await closeTab(dbg, "bundle.js");

  await resume(dbg);

  info("Add a second breakpoint");
  await addBreakpoint(dbg, "original.js", 13);

  is(dbg.selectors.getBreakpointCount(dbg), 2, "Two breakpoints exist");
  is(
    dbg.client.getServerBreakpointsList().length,
    2,
    "Two breakpoint exists on the server"
  );

  info("Check that the original location of the new breakpoint is correct");
  breakpoint = dbg.selectors.getBreakpointsList(dbg)[1];
  is(breakpoint.location.line, 13);
  if (isCompressed) {
    is(breakpoint.generatedLocation.line, 1);
    is(breakpoint.generatedLocation.column, 1089);
  } else {
    is(breakpoint.generatedLocation.line, 86);
  }

  // NOTE: When we reload, the `foo` function no longer exists
  // and the original.js file is now 3 lines long
  info("Reload and observe no breakpoints");
  testServer.switchToNextVersion();
  await reload(dbg, "original.js");

  // There will initially be zero breakpoints, but wait to make sure none are
  // installed while syncing.
  await wait(1000);

  assertNotPaused(dbg);
  is(dbg.selectors.getBreakpointCount(dbg), 0, "No breakpoints");
  // TODO: fails intermitently, look to fix
  /*is(
    dbg.client.getServerBreakpointsList().length,
    2,
    "No breakpoint exists on the server"
  );*/
});
