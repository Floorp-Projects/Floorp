/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the paused overlay in a remote frame

const TEST_COM_URI =
  URL_ROOT_COM + "examples/doc_dbg-fission-frame-sources.html";

add_task(async function() {
  info("Load a test page with a remote frame");
  // simple1.js is imported by the main page. simple2.js comes from the remote frame.
  const dbg = await initDebuggerWithAbsoluteURL(
    TEST_COM_URI,
    "simple1.js",
    "simple2.js"
  );
  const {
    selectors: { getSelectedSource },
    getState,
    commands,
  } = dbg;

  info("Add breakpoint within the iframe, in `foo` function");
  await selectSource(dbg, "simple2.js");
  await addBreakpoint(dbg, "simple2.js", 5);

  info("Call `foo` in the iframe");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const iframe = content.document.querySelector("iframe");
    SpecialPowers.spawn(iframe, [], () => {
      return content.wrappedJSObject.foo();
    });
  });
  await waitFor(() => isPaused(dbg), "Wait for the debugger to pause");
  ok(true, "debugger is paused");

  let testFront;
  if (isFissionEnabled()) {
    // We need to retrieve the testFront for the remote frame target.
    const iframeTarget = commands.targetCommand
      .getAllTargets([commands.targetCommand.TYPES.FRAME])
      .find(target => target.url.includes("example.org"));
    testFront = await iframeTarget.getFront("test");
  } else {
    // When fission is disabled, we don't have a dedicated target for the remote frame.
    // In this case, the overlay is displayed by the top-level target anyway, so we can
    // get the corresponding test front.
    testFront = await getTestActor(dbg.toolbox);
  }

  info("Check that the paused overlay is displayed");
  await waitFor(async () => await testFront.isPausedDebuggerOverlayVisible());
  ok(true, "Paused debugger overlay is visible");

  ok(
    getSelectedSource().url.includes("simple2.js"),
    "Selected source is simple2.js"
  );
  assertPausedLocation(dbg);
  assertDebugLine(dbg, 5);

  info("Test clicking the resume button");
  await testFront.clickPausedDebuggerOverlayButton("paused-dbg-resume-button");

  await waitFor(() => !isPaused(dbg), "Wait for the debugger to resume");
  ok("The debugger isn't paused after clicking on the resume button");

  await waitFor(async () => {
    const visible = await testFront.isPausedDebuggerOverlayVisible();
    return !visible;
  });

  ok(true, "The overlay is now hidden");
});
