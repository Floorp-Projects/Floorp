/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API around frames

const FISSION_TEST_URL = URL_ROOT_SSL + "fission_document.html";
const IFRAME_URL = URL_ROOT_ORG_SSL + "fission_iframe.html";
const SECOND_PAGE_URL = "https://example.org/document-builder.sjs?html=org";

add_task(async function() {
  // Disable bfcache for Fission for now.
  // If Fission is disabled, the pref is no-op.
  await SpecialPowers.pushPrefEnv({
    set: [["fission.bfcacheInParent", false]],
  });

  // Enabled fission prefs
  await pushPref("devtools.browsertoolbox.fission", true);
  // Disable the preloaded process as it gets created lazily and may interfere
  // with process count assertions
  await pushPref("dom.ipc.processPrelaunch.enabled", false);
  // This preference helps destroying the content process when we close the tab
  await pushPref("dom.ipc.keepProcessesAlive.web", 1);

  // Test fetching the frames from the main process descriptor
  await testBrowserFrames();

  // Test fetching the frames from a tab descriptor
  await testTabFrames();

  // Test what happens with documents running in the parent process
  await testOpeningOnParentProcessDocument();
  await testNavigationToParentProcessDocument();
});

async function testOpeningOnParentProcessDocument() {
  info("Test opening against a parent process document");
  const tab = await addTab("about:robots");
  is(
    tab.linkedBrowser.browsingContext.currentWindowGlobal.osPid,
    -1,
    "The tab is loaded in the parent process"
  );

  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();

  const frames = await targetCommand.getAllTargets([targetCommand.TYPES.FRAME]);
  is(frames.length, 1);
  is(frames[0].url, "about:robots", "target url is correct");
  is(
    frames[0],
    targetCommand.targetFront,
    "the target is the current top level one"
  );
}

async function testNavigationToParentProcessDocument() {
  info("Test navigating to parent process document");
  const firstLocation = "data:text/html,foo";
  const secondLocation = "about:robots";

  const tab = await addTab(firstLocation);
  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;
  // When the first top level target is created from the server,
  // `startListening` emits a spurious switched-target event
  // which isn't necessarily emited before it resolves.
  // So ensure waiting for it, otherwise we may resolve too eagerly
  // in our expected listener.
  const onSwitchedTarget1 = targetCommand.once("switched-target");
  await targetCommand.startListening();
  if (isServerTargetSwitchingEnabled()) {
    info("wait for first top level target");
    await onSwitchedTarget1;
  }

  const firstTarget = targetCommand.targetFront;
  is(firstTarget.url, firstLocation, "first target url is correct");

  info("Navigate to a parent process page");
  const onSwitchedTarget = targetCommand.once("switched-target");
  const browser = tab.linkedBrowser;
  const onLoaded = BrowserTestUtils.browserLoaded(browser);
  await BrowserTestUtils.loadURI(browser, secondLocation);
  await onLoaded;
  is(
    browser.browsingContext.currentWindowGlobal.osPid,
    -1,
    "The tab is loaded in the parent process"
  );

  await onSwitchedTarget;
  isnot(targetCommand.targetFront, firstTarget, "got a new target");

  // Check that calling getAllTargets([frame]) return the same target instances
  const frames = await targetCommand.getAllTargets([targetCommand.TYPES.FRAME]);
  is(frames.length, 1);
  is(frames[0].url, secondLocation, "second target url is correct");
  is(
    frames[0],
    targetCommand.targetFront,
    "second target is the current top level one"
  );
}

async function testBrowserFrames() {
  info("Test TargetCommand against frames via the parent process target");

  const commands = await CommandsFactory.forMainProcess();
  const targetCommand = commands.targetCommand;
  const { TYPES } = targetCommand;
  await targetCommand.startListening();

  // Very naive sanity check against getAllTargets([frame])
  const frames = await targetCommand.getAllTargets([TYPES.FRAME]);
  const hasBrowserDocument = frames.find(
    frameTarget => frameTarget.url == window.location.href
  );
  ok(hasBrowserDocument, "retrieve the target for the browser document");

  // Check that calling getAllTargets([frame]) return the same target instances
  const frames2 = await targetCommand.getAllTargets([TYPES.FRAME]);
  is(frames2.length, frames.length, "retrieved the same number of frames");

  function sortFronts(f1, f2) {
    return f1.actorID < f2.actorID;
  }
  frames.sort(sortFronts);
  frames2.sort(sortFronts);
  for (let i = 0; i < frames.length; i++) {
    is(frames[i], frames2[i], `frame ${i} targets are the same`);
  }

  // Assert that watchTargets will call the create callback for all existing frames
  const targets = [];
  const topLevelTarget = targetCommand.targetFront;
  const onAvailable = ({ targetFront }) => {
    is(
      targetFront.targetType,
      TYPES.FRAME,
      "We are only notified about frame targets"
    );
    ok(
      targetFront == topLevelTarget
        ? targetFront.isTopLevel
        : !targetFront.isTopLevel,
      "isTopLevel property is correct"
    );
    targets.push(targetFront);
  };
  await targetCommand.watchTargets([TYPES.FRAME], onAvailable);
  is(
    targets.length,
    frames.length,
    "retrieved the same number of frames via watchTargets"
  );

  frames.sort(sortFronts);
  targets.sort(sortFronts);
  for (let i = 0; i < frames.length; i++) {
    is(
      frames[i],
      targets[i],
      `frame ${i} targets are the same via watchTargets`
    );
  }

  async function addTabAndAssertNewTarget(url) {
    const previousTargetCount = targets.length;
    const tab = await addTab(url);
    await waitFor(
      () => targets.length == previousTargetCount + 1,
      "Wait for all expected targets after tab opening"
    );
    is(
      targets.length,
      previousTargetCount + 1,
      "Opening a tab reported a new frame"
    );
    is(
      targets[targets.length - 1].url,
      url,
      "This frame target is about the new tab"
    );

    const frames3 = await targetCommand.getAllTargets([TYPES.FRAME]);
    const hasTabDocument = frames3.find(target => target.url == url);
    ok(hasTabDocument, "retrieve the target for tab via getAllTargets");

    return tab;
  }

  info("Open a tab loaded in content process");
  await addTabAndAssertNewTarget("data:text/html,conten-process-page");

  info("Open a tab loaded in the parent process");
  const parentProcessTab = await addTabAndAssertNewTarget("about:robots");
  is(
    parentProcessTab.linkedBrowser.browsingContext.currentWindowGlobal.osPid,
    -1,
    "The tab is loaded in the parent process"
  );

  // Until we start spawning target for all WindowGlobals,
  // including the one running in the same process as their parent,
  // we won't create dedicated target for new top level windows.
  // Instead, these document will be debugged via the ParentProcessTargetActor.
  info("Open a top level chrome window");
  const expectedTargets = targets.length;
  const chromeWindow = Services.ww.openWindow(
    null,
    "about:robots",
    "_blank",
    "chrome",
    null
  );
  await wait(250);
  is(
    targets.length,
    expectedTargets,
    "New top level window shouldn't spawn new target"
  );
  chromeWindow.close();

  targetCommand.unwatchTargets([TYPES.FRAME], onAvailable);

  targetCommand.destroy();
  await waitForAllTargetsToBeAttached(targetCommand);

  await commands.destroy();
}

async function testTabFrames(mainRoot) {
  info("Test TargetCommand against frames via a tab target");

  // Create a TargetCommand for a given test tab
  const tab = await addTab(FISSION_TEST_URL);
  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;
  const { TYPES } = targetCommand;

  await targetCommand.startListening();

  // Check that calling getAllTargets([frame]) return the same target instances
  const frames = await targetCommand.getAllTargets([TYPES.FRAME]);
  // When fission is enabled, we also get the remote example.org iframe.
  const expectedFramesCount = isFissionEnabled() ? 2 : 1;
  is(
    frames.length,
    expectedFramesCount,
    "retrieved only the top level document"
  );

  // Assert that watchTargets will call the create callback for all existing frames
  const targets = [];
  const destroyedTargets = [];
  const topLevelTarget = targetCommand.targetFront;
  const onAvailable = ({ targetFront, isTargetSwitching }) => {
    is(
      targetFront.targetType,
      TYPES.FRAME,
      "We are only notified about frame targets"
    );
    targets.push({ targetFront, isTargetSwitching });
  };
  const onDestroyed = ({ targetFront, isTargetSwitching }) => {
    is(
      targetFront.targetType,
      TYPES.FRAME,
      "We are only notified about frame targets"
    );
    ok(
      targetFront == topLevelTarget
        ? targetFront.isTopLevel
        : !targetFront.isTopLevel,
      "isTopLevel property is correct"
    );
    destroyedTargets.push({ targetFront, isTargetSwitching });
  };
  await targetCommand.watchTargets([TYPES.FRAME], onAvailable, onDestroyed);
  is(
    targets.length,
    frames.length,
    "retrieved the same number of frames via watchTargets"
  );
  is(destroyedTargets.length, 0, "Should be no destroyed target initialy");

  for (const frame of frames) {
    ok(
      targets.find(({ targetFront }) => targetFront === frame),
      "frame " + frame.actorID + " target is the same via watchTargets"
    );
  }
  is(
    targets[0].targetFront.url,
    FISSION_TEST_URL,
    "First target should be the top document one"
  );
  is(
    targets[0].targetFront.isTopLevel,
    true,
    "First target is a top level one"
  );
  is(
    !targets[0].isTargetSwitching,
    true,
    "First target is not considered as a target switching"
  );
  if (isFissionEnabled()) {
    is(
      targets[1].targetFront.url,
      IFRAME_URL,
      "Second target should be the iframe one"
    );
    is(
      !targets[1].targetFront.isTopLevel,
      true,
      "Iframe target isn't top level"
    );
    is(
      !targets[1].isTargetSwitching,
      true,
      "Iframe target isn't a target swich"
    );
  }

  // Before navigating to another process, ensure cleaning up everything from the first page
  await waitForAllTargetsToBeAttached(targetCommand);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // registrationPromise is set by the test page.
    const registration = await content.wrappedJSObject.registrationPromise;
    registration.unregister();
  });

  info("Navigate to another domain and process (if fission is enabled)");
  // When a new target will be created, we need to wait until it's fully processed
  // to avoid pending promises.
  const onNewTargetProcessed =
    isFissionEnabled() || isServerTargetSwitchingEnabled()
      ? targetCommand.once("processed-available-target")
      : null;

  const browser = tab.linkedBrowser;
  const onLoaded = BrowserTestUtils.browserLoaded(browser);
  await BrowserTestUtils.loadURI(browser, SECOND_PAGE_URL);
  await onLoaded;

  if (isFissionEnabled()) {
    const afterNavigationFramesCount = 3;
    await waitFor(
      () => targets.length == afterNavigationFramesCount,
      "Wait for all expected targets after navigation"
    );
    is(
      targets.length,
      afterNavigationFramesCount,
      "retrieved all targets after navigation"
    );
    // As targetFront.url isn't reliable and might be about:blank,
    // try to assert that we got the right target via other means.
    // outerWindowID should change when navigating to another process,
    // while it would stay equal for in-process navigations.
    is(
      targets[2].targetFront.outerWindowID,
      browser.outerWindowID,
      "The new target should be the newly loaded document"
    );
    is(
      targets[2].isTargetSwitching,
      true,
      "and should be flagged as a target switching"
    );

    is(
      destroyedTargets.length,
      2,
      "The two existing targets should be destroyed"
    );
    is(
      destroyedTargets[0].targetFront,
      targets[0].targetFront,
      "The first destroyed should be the previous top level one"
    );
    is(
      destroyedTargets[0].isTargetSwitching,
      true,
      "the target destruction is flagged as target switching"
    );

    is(
      destroyedTargets[1].targetFront,
      targets[1].targetFront,
      "The second destroyed should be the iframe one"
    );
    is(
      destroyedTargets[1].isTargetSwitching,
      false,
      "the target destruction is not flagged as target switching for iframes"
    );
  } else if (isServerTargetSwitchingEnabled()) {
    await waitFor(
      () => targets.length == 2,
      "Wait for all expected targets after navigation"
    );
    is(
      destroyedTargets.length,
      1,
      "with JSWindowActor based target, the top level target is destroyed"
    );
    is(
      targetCommand.targetFront,
      targets[1].targetFront,
      "we got a new target"
    );
    ok(
      !targetCommand.targetFront.isDestroyed(),
      "that target is not destroyed"
    );
    ok(
      targets[0].targetFront.isDestroyed(),
      "but the previous one is destroyed"
    );
  } else {
    is(targets.length, 1, "without fission, we always have only one target");
    is(destroyedTargets.length, 0, "no target should be destroyed");
    is(
      targetCommand.targetFront,
      targets[0].targetFront,
      "and that unique target is always the same"
    );
    ok(
      !targetCommand.targetFront.isDestroyed(),
      "and that target is never destroyed"
    );
  }

  await onNewTargetProcessed;

  targetCommand.unwatchTargets([TYPES.FRAME], onAvailable);

  targetCommand.destroy();

  BrowserTestUtils.removeTab(tab);

  await commands.destroy();
}
