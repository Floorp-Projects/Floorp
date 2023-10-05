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
  await addBreakpoint(dbg, "original.js", 8);

  info("Check that only one breakpoint is set");
  is(dbg.selectors.getBreakpointCount(), 1, "Only one breakpoint exists");
  is(
    dbg.client.getServerBreakpointsList().length,
    1,
    "One breakpoint exists on the server"
  );

  info("Check that the breakpoint location info is correct");
  let breakpoint = dbg.selectors.getBreakpointsList(dbg)[0];
  is(breakpoint.location.line, 8);
  if (isCompressed) {
    is(breakpoint.generatedLocation.line, 1);
    is(breakpoint.generatedLocation.column, 1056);
  } else {
    is(breakpoint.generatedLocation.line, 84);
  }

  const expectedOriginalFileContentOnBreakpointLine =
    "await nonSourceMappedFunction();";
  const expectedGeneratedFileContentOnBreakpointLine =
    "await nonSourceMappedFunction();";

  info("Check that the breakpoint is displayed on the correct line in the ui");
  await assertBreakpoint(dbg, 8);

  info("Check that breakpoint is on the first line within the function `foo`");
  assertTextContentOnLine(dbg, 8, expectedOriginalFileContentOnBreakpointLine);

  info(
    "Check that the source text snippet displayed in breakpoints panel is correct"
  );
  assertBreakpointSnippet(
    dbg,
    1,
    isCompressed
      ? "nonSourceMappedFunction();"
      : "await nonSourceMappedFunction();"
  );

  info(
    "Check that the breakpoint is displayed in correct location in bundle.js (generated source)"
  );
  await selectSource(dbg, "bundle.js");
  if (isCompressed) {
    await assertBreakpoint(dbg, 1);
  } else {
    await assertBreakpoint(dbg, 84);
    assertTextContentOnLine(
      dbg,
      84,
      expectedGeneratedFileContentOnBreakpointLine
    );
  }
  info(
    "The breakpoint snippet doesn't change when moving to generated content"
  );
  assertBreakpointSnippet(
    dbg,
    1,
    isCompressed
      ? `nonSourceMappedFunction(),console.log("YO")}}]);`
      : "await nonSourceMappedFunction();"
  );

  await closeTab(dbg, "bundle.js");

  // This reload changes the content of the original file
  // which will cause the location of the breakpoint to change
  info("Reload with a new version of the file");
  testServer.switchToNextVersion();
  await reload(dbg, "bundle.js", "original.js");
  await wait(1000);

  info(
    "Check that no breakpoint is restore as original line 6 is no longer breakable"
  );
  is(dbg.selectors.getBreakpointCount(), 0, "No breakpoint exists");

  info("Invoke `foo` to trigger breakpoint");
  invokeInTab("foo");
  await wait(1000);

  // TODO: Intermittently pauses (especially when in compressed)
  // Need to investigate
  if (isPaused(dbg)) {
    await resume(dbg);
  }
  assertNotPaused(dbg);

  await closeTab(dbg, "bundle.js");

  info("Add a second breakpoint");
  await addBreakpoint(dbg, "original.js", 13);

  is(dbg.selectors.getBreakpointCount(dbg), 1, "The breakpoint exist");

  info("Check that the original location of the new breakpoint is correct");
  breakpoint = dbg.selectors.getBreakpointsList(dbg)[0];
  is(breakpoint.location.line, 13);
  if (isCompressed) {
    is(breakpoint.generatedLocation.line, 1);
    is(breakpoint.generatedLocation.column, 1089);
  } else {
    is(breakpoint.generatedLocation.line, 89);
  }
  assertBreakpointSnippet(
    dbg,
    1,
    isCompressed ? `log("HEY")` : `console.log("HEY")`
  );

  // This reload removes the content related to the lines in the original
  // file where the breakpoints where set.
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
