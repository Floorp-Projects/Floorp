/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * This tests resyncing of breakpoints in sourcemapped files when
 * source content changes of reload.
 */

"use strict";

const testServer = createVersionizedHttpTestServer("sourcemaps-reload");
const TEST_URL = testServer.urlFor("index.html");

/**
 * This first test will focus on original.js file whose content changes
 * which affects the related generated file: bundle.js
 */
add_task(async function testReloadingStableOriginalSource() {
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URL, "original.js");

  info("Add initial breakpoint");
  await selectSource(dbg, "original.js");
  await addBreakpoint(dbg, "original.js", 6);

  info("Check that only one breakpoint is set");
  is(dbg.selectors.getBreakpointCount(), 1, "Only one breakpoint exists");

  info("Check that the breakpoint location info is correct");
  let breakpoint = dbg.selectors.getBreakpointsList(dbg)[0];
  is(breakpoint.location.line, 6);
  is(breakpoint.generatedLocation.line, 82);

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
  await assertBreakpoint(dbg, 82);
  assertTextContentOnLine(
    dbg,
    82,
    expectedGeneratedFileContentOnBreakpointLine
  );

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

  info("Check that the original location has changed");
  breakpoint = dbg.selectors.getBreakpointsList(dbg)[0];
  is(breakpoint.location.line, 9);
  is(breakpoint.generatedLocation.line, 82);

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
  await assertPausedAtSourceAndLine(dbg, generatedSource.id, 82);
  await assertBreakpoint(dbg, 82);
  assertTextContentOnLine(
    dbg,
    82,
    expectedGeneratedFileContentOnBreakpointLine
  );

  await closeTab(dbg, "bundle.js");

  await resume(dbg);

  info("Add a second breakpoint");
  await addBreakpoint(dbg, "original.js", 13);

  is(dbg.selectors.getBreakpointCount(dbg), 2, "Two breakpoints exist");

  info("Check that the original location of the new breakpoint is correct");
  breakpoint = dbg.selectors.getBreakpointsList(dbg)[1];
  is(breakpoint.location.line, 13);
  is(breakpoint.generatedLocation.line, 86);

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
});

/**
 * This second test will focus on removed-original.js which is an original source mapped file.
 * This source is mapped to replaced-bundle.js.
 * This original source is removed and another original file: new-original.js
 * will replace the content of the removed-original.js in the replaced-bundle.js generated file.
 * And finally, everything is removed, both original and generated source.
 */
add_task(async function testReloadingReplacedOriginalSource() {
  testServer.backToFirstVersion();

  const dbg = await initDebuggerWithAbsoluteURL(
    TEST_URL,
    "removed-original.js"
  );

  info("Add initial breakpoint");
  await selectSource(dbg, "removed-original.js");
  await addBreakpoint(dbg, "removed-original.js", 2);

  // Assert the precise behavior of the breakpoint before reloading
  invokeInTab("removedOriginal");
  await waitForPaused(dbg);
  const replacedSource = findSource(dbg, "removed-original.js");
  assertPausedAtSourceAndLine(dbg, replacedSource.id, 2);
  assertTextContentOnLine(dbg, 2, 'console.log("Removed original");');
  await assertBreakpoint(dbg, 2);
  is(dbg.selectors.getBreakpointCount(), 1, "One breakpoint exists");
  let breakpoint = dbg.selectors.getBreakpointsList()[0];
  is(breakpoint.location.sourceUrl, replacedSource.url);
  is(breakpoint.location.line, 2);
  is(breakpoint.generatedLocation.line, 78);

  await resume(dbg);

  info(
    "Reload, which should remove the original file and a add a new original file which will replace its content in the  generated file"
  );
  const syncBp = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  testServer.switchToNextVersion();
  await reload(dbg);
  await syncBp;

  // Assert the new breakpoint being created after reload
  // For now, the current behavior of the debugger is that:
  // the breakpoint is still hit based on the generated source/bundle file
  // and the UI updates itself to mention the new original file.
  await waitForPaused(dbg);
  const newSource = findSource(dbg, "new-original.js");
  assertPausedAtSourceAndLine(dbg, newSource.id, 2);
  assertTextContentOnLine(dbg, 2, 'console.log("New original");');
  await assertBreakpoint(dbg, 2);
  is(dbg.selectors.getBreakpointCount(), 1, "One breakpoint exists");
  breakpoint = dbg.selectors.getBreakpointsList()[0];
  is(breakpoint.location.sourceUrl, newSource.url);
  is(breakpoint.location.line, 2);
  is(breakpoint.generatedLocation.line, 78);

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
});
