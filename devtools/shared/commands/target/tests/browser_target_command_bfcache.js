/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API when bfcache navigations happen

const TEST_COM_URL = URL_ROOT_SSL + "simple_document.html";

add_task(async function() {
  // Enabled fission prefs
  await pushPref("devtools.browsertoolbox.fission", true);
  // Disable the preloaded process as it gets created lazily and may interfere
  // with process count assertions
  await pushPref("dom.ipc.processPrelaunch.enabled", false);
  // This preference helps destroying the content process when we close the tab
  await pushPref("dom.ipc.keepProcessesAlive.web", 1);

  info("### Test with server side target switching");
  await pushPref("devtools.target-switching.server.enabled", true);
  await bfcacheTest();
});

async function bfcacheTest() {
  info("## Test with bfcache in parent DISABLED");
  await pushPref("fission.bfcacheInParent", false);
  await testTopLevelNavigations(false);
  await testIframeNavigations(false);
  await testTopLevelNavigationsOnDocumentWithIframe(false);

  // bfcacheInParent only works if sessionHistoryInParent is enable
  // so only test it if both settings are enabled.
  // (it looks like sessionHistoryInParent is enabled by default when fission is enabled)
  if (Services.appinfo.sessionHistoryInParent) {
    info("## Test with bfcache in parent ENABLED");
    await pushPref("fission.bfcacheInParent", true);
    await testTopLevelNavigations(true);
    await testIframeNavigations(true);
    await testTopLevelNavigationsOnDocumentWithIframe(true);
  }
}

async function testTopLevelNavigations(bfcacheInParent) {
  info(" # Test TOP LEVEL navigations");
  // Create a TargetCommand for a given test tab
  const tab = await addTab(TEST_COM_URL);
  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;
  const { TYPES } = targetCommand;

  await targetCommand.startListening();

  // Assert that watchTargets will call the create callback for all existing frames
  const targets = [];
  const onAvailable = async ({ targetFront }) => {
    is(
      targetFront.targetType,
      TYPES.FRAME,
      "We are only notified about frame targets"
    );
    ok(targetFront.isTopLevel, "all targets of this test are top level");
    targets.push(targetFront);
  };
  const destroyedTargets = [];
  const onDestroyed = async ({ targetFront }) => {
    is(
      targetFront.targetType,
      TYPES.FRAME,
      "We are only notified about frame targets"
    );
    ok(targetFront.isTopLevel, "all targets of this test are top level");
    destroyedTargets.push(targetFront);
  };

  await targetCommand.watchTargets({
    types: [TYPES.FRAME],
    onAvailable,
    onDestroyed,
  });
  is(targets.length, 1, "retrieved only the top level target");
  is(targets[0], targetCommand.targetFront, "the target is the top level one");
  is(
    destroyedTargets.length,
    0,
    "We get no destruction when calling watchTargets"
  );
  if (!isServerTargetSwitchingEnabled()) {
    ok(
      !targets[0].targetForm.followWindowGlobalLifeCycle,
      "the first client side target still follows docshell lifecycle, when server target switching isn't enabled"
    );
  } else {
    ok(
      targets[0].targetForm.followWindowGlobalLifeCycle,
      "the first server side target follows the WindowGlobal lifecycle, when server target switching is enabled"
    );
  }

  // Navigate to the same page with query params
  info("Load the second page");
  let onDomComplete = bfcacheInParent
    ? null
    : (await waitForNextTopLevelDomCompleteResource(commands))
        .onDomCompleteResource;
  const secondPageUrl = TEST_COM_URL + "?second-load";
  const previousBrowsingContextID = gBrowser.selectedBrowser.browsingContext.id;
  ok(
    previousBrowsingContextID,
    "Fetch the tab's browsing context id before navigation"
  );
  const onLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    secondPageUrl
  );
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, secondPageUrl);
  await onLoaded;

  // Assert BrowsingContext changes as it impact the behavior of targets
  if (bfcacheInParent) {
    isnot(
      previousBrowsingContextID,
      gBrowser.selectedBrowser.browsingContext.id,
      "When bfcacheInParent is enabled, same-origin navigations spawn new BrowsingContext"
    );
  } else {
    is(
      previousBrowsingContextID,
      gBrowser.selectedBrowser.browsingContext.id,
      "When bfcacheInParent is disabled, same-origin navigations re-use the same BrowsingContext"
    );
  }

  if (bfcacheInParent || isServerTargetSwitchingEnabled()) {
    // When server side target switching is enabled, same-origin navigations also spawn a new top level target
    await waitFor(
      () => targets.length == 2,
      "wait for the next top level target"
    );
    is(
      targets[1],
      targetCommand.targetFront,
      "the second target is the top level one"
    );
    // As targetFront.url isn't reliable and might be about:blank,
    // try to assert that we got the right target via other means.
    // outerWindowID should change when navigating to another process,
    // while it would stay equal for in-process navigations.
    is(
      targets[1].outerWindowID,
      gBrowser.selectedBrowser.outerWindowID,
      "the second target is for the second page"
    );
    if (!isServerTargetSwitchingEnabled()) {
      ok(
        !targets[1].targetForm.followWindowGlobalLifeCycle,
        "the new client side target still follows docshell lifecycle"
      );
    } else {
      ok(
        targets[1].targetForm.followWindowGlobalLifeCycle,
        "the new server side target follows the WindowGlobal lifecycle"
      );
    }
    ok(targets[0].isDestroyed(), "the first target is destroyed");
    is(destroyedTargets.length, 1, "We get one target being destroyed...");
    is(destroyedTargets[0], targets[0], "...and that's the first one");
  } else {
    info("Wait for 'dom-complete' resource");
    await onDomComplete;
  }

  // Go back to the first page, this should be a bfcache navigation, and,
  // we should get a new target
  info("Go back to the first page");
  onDomComplete = bfcacheInParent
    ? null
    : (await waitForNextTopLevelDomCompleteResource(commands))
        .onDomCompleteResource;
  gBrowser.selectedBrowser.goBack();

  if (bfcacheInParent || isServerTargetSwitchingEnabled()) {
    await waitFor(
      () => targets.length == 3,
      "wait for the next top level target"
    );
    is(
      targets[2],
      targetCommand.targetFront,
      "the third target is the top level one"
    );
    // Here as this is revived from cache, the url should always be correct
    is(targets[2].url, TEST_COM_URL, "the third target is for the first url");
    ok(
      targets[2].targetForm.followWindowGlobalLifeCycle,
      "the third target for bfcache navigations is following the WindowGlobal lifecycle"
    );
    ok(targets[1].isDestroyed(), "the second target is destroyed");
    is(
      destroyedTargets.length,
      2,
      "We get one additional target being destroyed..."
    );
    is(destroyedTargets[1], targets[1], "...and that's the second one");

    // Wait for full attach in order to having breaking any pending requests
    // when navigating to another page and switching to new process and target.
    await waitForAllTargetsToBeAttached(targetCommand);
  } else {
    info("Wait for 'dom-complete' resource");
    await onDomComplete;
  }

  // Go forward and resurect the second page, this should also be a bfcache navigation, and,
  // get a new target.
  info("Go forward to the second page");

  onDomComplete = bfcacheInParent
    ? null
    : (await waitForNextTopLevelDomCompleteResource(commands))
        .onDomCompleteResource;

  // When a new target will be created, we need to wait until it's fully processed
  // to avoid pending promises.
  const onNewTargetProcessed = bfcacheInParent
    ? new Promise(resolve => {
        targetCommand.on(
          "processed-available-target",
          function onProcessedAvailableTarget(targetFront) {
            if (targetFront === targets[3]) {
              resolve();
              targetCommand.off(
                "processed-available-target",
                onProcessedAvailableTarget
              );
            }
          }
        );
      })
    : null;

  gBrowser.selectedBrowser.goForward();

  if (bfcacheInParent || isServerTargetSwitchingEnabled()) {
    await waitFor(
      () => targets.length == 4,
      "wait for the next top level target"
    );
    is(
      targets[3],
      targetCommand.targetFront,
      "the 4th target is the top level one"
    );
    // Same here, as the document is revived from the cache, the url should always be correct
    is(targets[3].url, secondPageUrl, "the 4th target is for the second url");
    ok(
      targets[3].targetForm.followWindowGlobalLifeCycle,
      "the 4th target for bfcache navigations is following the WindowGlobal lifecycle"
    );
    ok(targets[2].isDestroyed(), "the third target is destroyed");
    is(
      destroyedTargets.length,
      3,
      "We get one additional target being destroyed..."
    );
    is(destroyedTargets[2], targets[2], "...and that's the third one");

    // Wait for full attach in order to having breaking any pending requests
    // when navigating to another page and switching to new process and target.
    await waitForAllTargetsToBeAttached(targetCommand);
    await onNewTargetProcessed;
  } else {
    info("Wait for 'dom-complete' resource");
    await onDomComplete;
  }

  await waitForAllTargetsToBeAttached(targetCommand);

  targetCommand.unwatchTargets({ types: [TYPES.FRAME], onAvailable });

  BrowserTestUtils.removeTab(tab);

  await commands.destroy();
}

async function testTopLevelNavigationsOnDocumentWithIframe(bfcacheInParent) {
  info(" # Test TOP LEVEL navigations on document with iframe");
  // Create a TargetCommand for a given test tab
  const tab = await addTab(`https://example.com/document-builder.sjs?id=top&html=
    <h1>Top level</h1>
    <iframe src="${encodeURIComponent(
      "https://example.com/document-builder.sjs?id=iframe&html=<h2>In iframe</h2>"
    )}">
    </iframe>`);
  const getLocationIdParam = url =>
    new URLSearchParams(new URL(url).search).get("id");

  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;
  const { TYPES } = targetCommand;

  await targetCommand.startListening();

  // Assert that watchTargets will call the create callback for all existing frames
  const targets = [];
  const onAvailable = async ({ targetFront }) => {
    is(
      targetFront.targetType,
      TYPES.FRAME,
      "We are only notified about frame targets"
    );
    targets.push(targetFront);
  };
  const destroyedTargets = [];
  const onDestroyed = async ({ targetFront }) => {
    is(
      targetFront.targetType,
      TYPES.FRAME,
      "We are only notified about frame targets"
    );
    destroyedTargets.push(targetFront);
  };

  await targetCommand.watchTargets({
    types: [TYPES.FRAME],
    onAvailable,
    onDestroyed,
  });

  if (isEveryFrameTargetEnabled()) {
    is(
      targets.length,
      2,
      "retrieved targets for top level and iframe documents"
    );
    is(
      targets[0],
      targetCommand.targetFront,
      "the target is the top level one"
    );
    is(
      getLocationIdParam(targets[1].url),
      "iframe",
      "the second target is the iframe one"
    );
  } else {
    is(targets.length, 1, "retrieved only the top level target");
    is(
      targets[0],
      targetCommand.targetFront,
      "the target is the top level one"
    );
  }

  is(
    destroyedTargets.length,
    0,
    "We get no destruction when calling watchTargets"
  );

  info("Navigate to a new page");
  let targetCountBeforeNavigation = targets.length;
  let onDomComplete = bfcacheInParent
    ? null
    : (await waitForNextTopLevelDomCompleteResource(commands))
        .onDomCompleteResource;
  const secondPageUrl = `https://example.com/document-builder.sjs?html=second`;
  const onLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    secondPageUrl
  );
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, secondPageUrl);
  await onLoaded;

  if (bfcacheInParent || isServerTargetSwitchingEnabled()) {
    // When server side target switching is enabled, same-origin navigations also spawn a new top level target
    await waitFor(
      () => targets.length == targetCountBeforeNavigation + 1,
      "wait for the next top level target"
    );
    is(
      targets.at(-1),
      targetCommand.targetFront,
      "the new target is the top level one"
    );

    ok(targets[0].isDestroyed(), "the first target is destroyed");
    if (isEveryFrameTargetEnabled()) {
      ok(targets[1].isDestroyed(), "the second target is destroyed");
      is(destroyedTargets.length, 2, "The two targets were destroyed");
    } else {
      is(destroyedTargets.length, 1, "Only one target was destroyed");
    }
  } else {
    info("Wait for 'dom-complete' resource");
    await onDomComplete;
  }

  // Go back to the first page, this should be a bfcache navigation, and,
  // we should get a new target (or 2 if EFT is enabled)
  targetCountBeforeNavigation = targets.length;
  info("Go back to the first page");
  onDomComplete = bfcacheInParent
    ? null
    : (await waitForNextTopLevelDomCompleteResource(commands))
        .onDomCompleteResource;
  gBrowser.selectedBrowser.goBack();

  if (bfcacheInParent || isServerTargetSwitchingEnabled()) {
    await waitFor(
      () =>
        targets.length ===
        targetCountBeforeNavigation + (isEveryFrameTargetEnabled() ? 2 : 1),
      "wait for the next top level target"
    );

    if (isEveryFrameTargetEnabled()) {
      await waitFor(() => targets.at(-2).url && targets.at(-1).url);
      is(
        getLocationIdParam(targets.at(-2).url),
        "top",
        "the first new target is for the top document…"
      );
      is(
        getLocationIdParam(targets.at(-1).url),
        "iframe",
        "…and the second one is for the iframe"
      );
    } else {
      is(
        getLocationIdParam(targets.at(-1).url),
        "top",
        "the new target is for the first url"
      );
    }

    ok(
      targets[targetCountBeforeNavigation - 1].isDestroyed(),
      "the target for the second page is destroyed"
    );
    is(
      destroyedTargets.length,
      targetCountBeforeNavigation,
      "We get one additional target being destroyed…"
    );
    is(
      destroyedTargets.at(-1),
      targets[targetCountBeforeNavigation - 1],
      "…and that's the second page one"
    );
  } else {
    info("Wait for 'dom-complete' resource");
    await onDomComplete;
  }

  await waitForAllTargetsToBeAttached(targetCommand);

  targetCommand.unwatchTargets({
    types: [TYPES.FRAME],
    onAvailable,
    onDestroyed,
  });

  BrowserTestUtils.removeTab(tab);

  await commands.destroy();
}

async function testIframeNavigations() {
  info(" # Test IFRAME navigations");
  // Create a TargetCommand for a given test tab
  const tab = await addTab(
    `http://example.org/document-builder.sjs?html=<iframe src="${TEST_COM_URL}"></iframe>`
  );
  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;
  const { TYPES } = targetCommand;

  await targetCommand.startListening();

  // Assert that watchTargets will call the create callback for all existing frames
  const targets = [];
  const onAvailable = async ({ targetFront }) => {
    is(
      targetFront.targetType,
      TYPES.FRAME,
      "We are only notified about frame targets"
    );
    targets.push(targetFront);
  };
  await targetCommand.watchTargets({ types: [TYPES.FRAME], onAvailable });

  // When fission/EFT is off, there isn't much to test for iframes as they are debugged
  // when the unique top level target
  if (!isFissionEnabled() && !isEveryFrameTargetEnabled()) {
    is(
      targets.length,
      1,
      "Without fission/EFT, there is only the top level target"
    );
    await commands.destroy();
    return;
  }
  is(targets.length, 2, "retrieved the top level and the iframe targets");
  is(
    targets[0],
    targetCommand.targetFront,
    "the first target is the top level one"
  );
  is(targets[1].url, TEST_COM_URL, "the second target is the iframe one");

  // Navigate to the same page with query params
  info("Load the second page");
  const secondPageUrl = TEST_COM_URL + "?second-load";
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [secondPageUrl], function(
    url
  ) {
    const iframe = content.document.querySelector("iframe");
    iframe.src = url;
  });

  await waitFor(() => targets.length == 3, "wait for the next target");
  is(targets[2].url, secondPageUrl, "the second target is for the second url");
  ok(targets[1].isDestroyed(), "the first target is destroyed");

  // Go back to the first page, this should be a bfcache navigation, and,
  // we should get a new target
  info("Go back to the first page");
  const iframeBrowsingContext = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function() {
      const iframe = content.document.querySelector("iframe");
      return iframe.browsingContext;
    }
  );
  await SpecialPowers.spawn(iframeBrowsingContext, [], function() {
    content.history.back();
  });

  await waitFor(() => targets.length == 4, "wait for the next target");
  is(targets[3].url, TEST_COM_URL, "the third target is for the first url");
  ok(targets[2].isDestroyed(), "the second target is destroyed");

  // Go forward and resurect the second page, this should also be a bfcache navigation, and,
  // get a new target.
  info("Go forward to the second page");
  await SpecialPowers.spawn(iframeBrowsingContext, [], function() {
    content.history.forward();
  });

  await waitFor(() => targets.length == 5, "wait for the next target");
  is(targets[4].url, secondPageUrl, "the 4th target is for the second url");
  ok(targets[3].isDestroyed(), "the third target is destroyed");

  targetCommand.unwatchTargets({ types: [TYPES.FRAME], onAvailable });

  await waitForAllTargetsToBeAttached(targetCommand);

  BrowserTestUtils.removeTab(tab);

  await commands.destroy();
}
