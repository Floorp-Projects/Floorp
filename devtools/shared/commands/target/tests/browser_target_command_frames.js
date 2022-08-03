/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API around frames

const FISSION_TEST_URL = URL_ROOT_SSL + "fission_document.html";
const IFRAME_URL = URL_ROOT_ORG_SSL + "fission_iframe.html";
const SECOND_PAGE_URL = "https://example.org/document-builder.sjs?html=org";

const PID_REGEXP = /^\d+$/;

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

  // Test what happens with about:blank documents
  await testOpeningOnAboutBlankDocument();
  await testNavigationToAboutBlankDocument();

  await testNestedIframes();
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

  await commands.destroy();
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

  await commands.destroy();
}

async function testOpeningOnAboutBlankDocument() {
  info("Test opening against about:blank document");
  const tab = await addTab("about:blank");

  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();

  const frames = await targetCommand.getAllTargets([targetCommand.TYPES.FRAME]);
  is(frames.length, 1);
  is(frames[0].url, "about:blank", "target url is correct");
  is(
    frames[0],
    targetCommand.targetFront,
    "the target is the current top level one"
  );

  await commands.destroy();
}

async function testNavigationToAboutBlankDocument() {
  info("Test navigating to about:blank");
  const firstLocation = "data:text/html,foo";
  const secondLocation = "about:blank";

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

  info("Navigate to about:blank page");
  const onSwitchedTarget = targetCommand.once("switched-target");
  const browser = tab.linkedBrowser;
  const onLoaded = BrowserTestUtils.browserLoaded(browser);
  await BrowserTestUtils.loadURI(browser, secondLocation);
  await onLoaded;

  if (isServerTargetSwitchingEnabled()) {
    await onSwitchedTarget;
    isnot(targetCommand.targetFront, firstTarget, "got a new target");

    // Check that calling getAllTargets([frame]) return the same target instances
    const frames = await targetCommand.getAllTargets([
      targetCommand.TYPES.FRAME,
    ]);
    is(frames.length, 1);
    is(frames[0].url, secondLocation, "second target url is correct");
    is(
      frames[0],
      targetCommand.targetFront,
      "second target is the current top level one"
    );
  } else {
    is(
      targetCommand.targetFront,
      firstTarget,
      "without server target switching, we stay on the same top level target"
    );
  }

  await commands.destroy();
}

async function testBrowserFrames() {
  info("Test TargetCommand against frames via the parent process target");

  const aboutBlankTab = await addTab("about:blank");

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

  const hasAboutBlankDocument = frames.find(
    frameTarget =>
      frameTarget.browsingContextID ==
      aboutBlankTab.linkedBrowser.browsingContext.id
  );
  ok(hasAboutBlankDocument, "retrieve the target for the about:blank tab");

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

  const noParentTarget = await topLevelTarget.getParentTarget();
  is(noParentTarget, null, "The top level target has no parent target");

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
    ok(
      PID_REGEXP.test(targetFront.processID),
      `Target has processID of expected shape (${targetFront.processID})`
    );
    targets.push(targetFront);
  };
  await targetCommand.watchTargets({ types: [TYPES.FRAME], onAvailable });
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
    const newTabTarget = targets.at(-1);
    is(newTabTarget.url, url, "This frame target is about the new tab");
    // Internaly, the tab, which uses a <browser type='content'> element is considered detached from their owner document
    // and so the target is having a null parentInnerWindowId. But the framework will attach all non-top-level targets
    // as children of the top level.
    const tabParentTarget = await newTabTarget.getParentTarget();
    is(
      tabParentTarget,
      targetCommand.targetFront,
      "tab's WindowGlobal/BrowsingContext is detached and has no parent, but we report them as children of the top level target"
    );

    const frames3 = await targetCommand.getAllTargets([TYPES.FRAME]);
    const hasTabDocument = frames3.find(target => target.url == url);
    ok(hasTabDocument, "retrieve the target for tab via getAllTargets");

    return tab;
  }

  info("Open a tab loaded in content process");
  await addTabAndAssertNewTarget("data:text/html,content-process-page");

  info("Open a tab loaded in the parent process");
  const parentProcessTab = await addTabAndAssertNewTarget("about:robots");
  is(
    parentProcessTab.linkedBrowser.browsingContext.currentWindowGlobal.osPid,
    -1,
    "The tab is loaded in the parent process"
  );

  info("Open a new content window via window.open");
  info("First open a tab on .org domain");
  const tabUrl = "https://example.org/document-builder.sjs?html=org";
  await addTabAndAssertNewTarget(tabUrl);
  const previousTargetCount = targets.length;

  info("Then open a popup on .com domain");
  const popupUrl = "https://example.com/document-builder.sjs?html=com";
  const onPopupOpened = BrowserTestUtils.waitForNewTab(gBrowser, popupUrl);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [popupUrl], async url => {
    content.window.open(url, "_blank");
  });
  await onPopupOpened;

  await waitFor(
    () => targets.length == previousTargetCount + 1,
    "Wait for all expected targets after window.open()"
  );
  is(
    targets.length,
    previousTargetCount + 1,
    "Opening a new content window reported a new frame"
  );
  is(
    targets.at(-1).url,
    popupUrl,
    "This frame target is about the new content window"
  );

  // About:blank are a bit special because we ignore a transcient about:blank
  // document when navigating to another process. But we should not ignore
  // tabs, loading a real, final about:blank document.
  info("Open a tab with about:blank");
  await addTabAndAssertNewTarget("about:blank");

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

  targetCommand.unwatchTargets({ types: [TYPES.FRAME], onAvailable });

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
  const expectedFramesCount =
    isFissionEnabled() || isEveryFrameTargetEnabled() ? 2 : 1;
  is(
    frames.length,
    expectedFramesCount,
    "retrieved the expected number of targets"
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
    ok(
      PID_REGEXP.test(targetFront.processID),
      `Target has processID of expected shape (${targetFront.processID})`
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
  await targetCommand.watchTargets({
    types: [TYPES.FRAME],
    onAvailable,
    onDestroyed,
  });
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
  const noParentTarget = await targets[0].targetFront.getParentTarget();
  is(noParentTarget, null, "The top level target has no parent target");

  if (isFissionEnabled() || isEveryFrameTargetEnabled()) {
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
    const parentTarget = await targets[1].targetFront.getParentTarget();
    is(
      parentTarget,
      targets[0].targetFront,
      "The parent target for the iframe is the top level target"
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

  if (isFissionEnabled() || isEveryFrameTargetEnabled()) {
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
      targets[1].targetFront,
      "The first destroyed should be the iframe one"
    );
    is(
      destroyedTargets[0].isTargetSwitching,
      false,
      "the target destruction is not flagged as target switching for iframes"
    );
    is(
      destroyedTargets[1].targetFront,
      targets[0].targetFront,
      "The second destroyed should be the previous top level one (because it is delayed to be fired *after* will-navigate)"
    );
    is(
      destroyedTargets[1].isTargetSwitching,
      true,
      "the target destruction is flagged as target switching"
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

  targetCommand.unwatchTargets({ types: [TYPES.FRAME], onAvailable });

  targetCommand.destroy();

  BrowserTestUtils.removeTab(tab);

  await commands.destroy();
}

async function testNestedIframes() {
  info("Test TargetCommand against nested frames");

  const nestedIframeUrl = `https://example.com/document-builder.sjs?html=${encodeURIComponent(
    "<title>second</title><h3>second level iframe</h3>"
  )}&delay=500`;

  const testUrl = `data:text/html;charset=utf-8,
    <h1>Top-level</h1>
    <iframe id=first-level
      src='data:text/html;charset=utf-8,${encodeURIComponent(
        `<title>first</title><h2>first level iframe</h2><iframe id=second-level src="${nestedIframeUrl}"></iframe>`
      )}'
    ></iframe>`;

  // Create a TargetCommand for a given test tab
  const tab = await addTab(testUrl);
  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;
  const { TYPES } = targetCommand;

  await targetCommand.startListening();

  // Check that calling getAllTargets([frame]) return the same target instances
  const frames = await targetCommand.getAllTargets([TYPES.FRAME]);

  is(frames[0], targetCommand.targetFront, "First target is the top level one");
  const topParent = await frames[0].getParentTarget();
  is(topParent, null, "Top level target has no parent");

  if (isEveryFrameTargetEnabled()) {
    const firstIframeTarget = frames.find(target => target.title == "first");
    ok(
      firstIframeTarget,
      "With EFT, got the target for the first level iframe"
    );
    const firstParent = await firstIframeTarget.getParentTarget();
    is(
      firstParent,
      targetCommand.targetFront,
      "With EFT, first level has top level target as parent"
    );

    const secondIframeTarget = frames.find(target => target.title == "second");
    ok(secondIframeTarget, "Got the target for the second level iframe");
    const secondParent = await secondIframeTarget.getParentTarget();
    is(
      secondParent,
      firstIframeTarget,
      "With EFT, second level has the first level target as parent"
    );
  } else if (isFissionEnabled()) {
    const secondIframeTarget = frames.find(target => target.title == "second");
    ok(secondIframeTarget, "Got the target for the second level iframe");
    const secondParent = await secondIframeTarget.getParentTarget();
    is(
      secondParent,
      targetCommand.targetFront,
      "With fission, second level has top level target as parent"
    );
  }

  await commands.destroy();
}
