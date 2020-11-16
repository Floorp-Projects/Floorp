/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test Watcher actor against frames.
 */

"use strict";

const TEST_DOC_URL = MAIN_DOMAIN + "doc_iframe.html";

// URL of the <iframe> already loaded in doc_iframe.html (the top level test document)
const TEST_DOC_URL1 =
  MAIN_DOMAIN.replace("test1.example.org", "example.com") +
  "doc_iframe_content.html";

// URL of an <iframe> dynamically inserted during the test
const TEST_DOC_URL2 =
  MAIN_DOMAIN.replace("test1.example.org", "example.net") + "doc_iframe2.html";

add_task(async function() {
  const tabTarget = await addTabTarget(TEST_DOC_URL);
  const { mainRoot } = tabTarget.client;

  // First test watching all frames. Really all of them.
  // From all top level windows, tabs, and any inner remoted iframe.
  await testWatchAllFrames(mainRoot);

  // Then use the Watcher to watch only frames of a given tab.
  await testWatchOneTabFrames(tabTarget);
});

async function testWatchAllFrames(mainRoot) {
  info("Assert watchTargets against the whole browser");
  const targets = [];

  const processDescriptor = await mainRoot.getMainProcess();
  const watcher = await processDescriptor.getWatcher();

  const onNewTarget = target => {
    dump(
      " (+) target: " + target.url + " -- " + target.browsingContextID + "\n"
    );
    targets.push(target);
  };
  watcher.on("target-available", onNewTarget);
  await watcher.watchTargets("frame");

  // As it retrieve the whole browser browsing contexts, it is hard to know how many it will fetch
  ok(targets.length > 0, "Got multiple frame targets");

  // Assert that we get a target for the top level tab document
  const tabTarget = targets.find(f => f.url == TEST_DOC_URL);
  ok(tabTarget, "We get the target for the tab document");
  const tabId = gBrowser.selectedTab.linkedBrowser.browsingContext.id;
  is(
    tabTarget.browsingContextID,
    tabId,
    "The tab frame target BrowsingContextID is correct"
  );

  const parentProcessTarget = await processDescriptor.getTarget();
  is(
    await parentProcessTarget.getWatcher(),
    watcher,
    "Parent process target getWatcher returns the same watcher"
  );

  const parentTarget = await tabTarget.getParentTarget();
  is(
    parentTarget,
    parentProcessTarget,
    "Tab parent target is the main process target"
  );

  const tabTarget2 = await watcher.getBrowsingContextTarget(tabId);
  is(tabTarget, tabTarget2, "getBrowsingContextTarget returns the same target");

  // Assert that we also fetch the iframe targets
  await assertTabIFrames(watcher, targets, tabTarget);

  await watcher.unwatchTargets("frame");
  watcher.off("target-available", onNewTarget);
}

async function testWatchOneTabFrames(tabTarget) {
  info("Assert watchTargets against a given Tab");
  const targets = [];

  const tabDescriptor = tabTarget.descriptorFront;
  const watcher = await tabDescriptor.getWatcher();

  is(
    await tabTarget.getWatcher(),
    watcher,
    "Tab target getWatcher returns the same watcher"
  );

  const onNewTarget = target => {
    dump(
      " (+) target: " + target.url + " -- " + target.browsingContextID + "\n"
    );
    targets.push(target);
  };
  watcher.on("target-available", onNewTarget);

  await watcher.watchTargets("frame");

  if (SpecialPowers.useRemoteSubframes) {
    is(targets.length, 1, "With fission, one additional target is reported");
  } else {
    is(targets.length, 0, "Without fission no additional target is reported");
  }

  await assertTabIFrames(watcher, targets, tabTarget);

  await watcher.unwatchTargets("frame");
  watcher.off("target-available", onNewTarget);
}

async function assertTabIFrames(watcher, targets, tabTarget) {
  // - The existing <iframe>
  const existingIframeTarget = targets.find(f => f.url === TEST_DOC_URL1);
  const existingIframeId = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    null,
    url => {
      const iframe = content.document.querySelector("#remote-frame");
      return iframe.frameLoader.browsingContext.id;
    }
  );

  if (SpecialPowers.useRemoteSubframes) {
    ok(existingIframeTarget, "And its remote child iframe");
    is(
      existingIframeTarget.browsingContextID,
      existingIframeId,
      "The iframe target BrowsingContextID is correct"
    );
    is(
      await existingIframeTarget.getWatcher(),
      watcher,
      "Iframe target getWatcher returns the same watcher"
    );
    const sameTarget = await watcher.getBrowsingContextTarget(existingIframeId);
    is(
      sameTarget,
      existingIframeTarget,
      "getBrowsingContextTarget returns the same target"
    );
    const sameTabTarget = await watcher.getParentBrowsingContextTarget(
      existingIframeId
    );
    is(
      tabTarget,
      sameTabTarget,
      "getParentBrowsingContextTarget returned the tab target"
    );
    const existingIframeParentTarget = await existingIframeTarget.getParentTarget();
    is(
      existingIframeParentTarget,
      tabTarget,
      "The iframe parent target is the tab target"
    );
  } else {
    ok(
      !existingIframeTarget,
      "Without fission, the iframes are not spawning additional targets"
    );
  }

  const originalFrameCount = targets.length;

  if (SpecialPowers.useRemoteSubframes) {
    // Assert that the frame target get destroyed and re-created on reload
    // This only happens with Fission, as otherwise, the iframe doesn't get a Descriptor/Target anyway.
    const onReloadedCreated = watcher.once("target-available");
    const onReloadedDestroyed = watcher.once("target-destroyed");
    SpecialPowers.spawn(BrowsingContext.get(existingIframeId), [], () => {
      content.location.reload();
    });
    info("Waiting for previous target destruction");
    const destroyedIframeTarget = await onReloadedDestroyed;
    info("Waiting for new target creation");
    const createdIframeTarget = await onReloadedCreated;
    is(
      destroyedIframeTarget,
      existingIframeTarget,
      "the destroyed target is the expected one, i.e. the previous one"
    );
    isnot(
      createdIframeTarget,
      existingIframeTarget,
      "the created target is the expected one, i.e. a new one"
    );
    is(
      createdIframeTarget.browsingContextID,
      existingIframeId,
      "The new iframe target BrowsingContextID is the same"
    );
    const createdIframeParentTarget = await createdIframeTarget.getParentTarget();
    is(
      createdIframeParentTarget,
      tabTarget,
      "The created iframe parent target is also the tab target"
    );
  }

  // Assert that we also get an event for iframes added after the call to watchTargets
  const onCreated = watcher.once("target-available");
  const newIframeId = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    TEST_DOC_URL2,
    url => {
      const iframe = content.document.createElement("iframe");
      iframe.src = url;
      content.document.body.appendChild(iframe);
      return iframe.frameLoader.browsingContext.id;
    }
  );

  if (SpecialPowers.useRemoteSubframes) {
    await onCreated;
    is(targets.length, originalFrameCount + 2, "Got the additional frame");

    // Match against the BrowsingContext ID as the target is created very early
    // and the url isn't set yet when the target is created.
    const newIframeTarget = targets.find(
      f => f.browsingContextID === newIframeId
    );
    ok(newIframeTarget, "We got the target for the new iframe");

    const newIframeParentTarget = await newIframeTarget.getParentTarget();
    is(
      newIframeParentTarget,
      tabTarget,
      "The new iframe parent target is also the tab target"
    );

    // Assert that we also get notified about destroyed frames
    const onDestroyed = watcher.once("target-destroyed");
    await ContentTask.spawn(gBrowser.selectedBrowser, newIframeId, id => {
      const browsingContext = BrowsingContext.get(id);
      const iframe = browsingContext.embedderElement;
      iframe.remove();
    });
    const destroyedTarget = await onDestroyed;
    is(
      destroyedTarget,
      newIframeTarget,
      "We got notified about the iframe destruction"
    );
  } else {
    is(
      targets.length,
      originalFrameCount,
      "Without fission, the iframes are not spawning additional targets"
    );
  }
}
