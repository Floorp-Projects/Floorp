async function waitForNoAnimation(elt) {
  return TestUtils.waitForCondition(() => !elt.hasAttribute("animate"));
}

async function getAnimatePromise(elt) {
  return BrowserTestUtils.waitForAttribute("animate", elt).then(() =>
    Assert.ok(true, `${elt.id} should animate`)
  );
}

function stopReloadMutationCallback() {
  Assert.ok(
    false,
    "stop-reload's animate attribute should not have been mutated"
  );
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["ui.prefersReducedMotion", 0]],
  });
});

add_task(async function checkDontShowStopOnNewTab() {
  let stopReloadContainer = document.getElementById("stop-reload-button");
  let stopReloadContainerObserver = new MutationObserver(
    stopReloadMutationCallback
  );

  await waitForNoAnimation(stopReloadContainer);
  stopReloadContainerObserver.observe(stopReloadContainer, {
    attributeFilter: ["animate"],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:robots",
    waitForStateStop: true,
  });
  BrowserTestUtils.removeTab(tab);

  Assert.ok(
    true,
    "Test finished: stop-reload does not animate when navigating to local URI on new tab"
  );
  stopReloadContainerObserver.disconnect();
});

add_task(async function checkDontShowStopFromLocalURI() {
  let stopReloadContainer = document.getElementById("stop-reload-button");
  let stopReloadContainerObserver = new MutationObserver(
    stopReloadMutationCallback
  );

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:robots",
    waitForStateStop: true,
  });
  await waitForNoAnimation(stopReloadContainer);
  stopReloadContainerObserver.observe(stopReloadContainer, {
    attributeFilter: ["animate"],
  });
  await BrowserTestUtils.loadURI(tab.linkedBrowser, "about:mozilla");
  BrowserTestUtils.removeTab(tab);

  Assert.ok(
    true,
    "Test finished: stop-reload does not animate when navigating between local URIs"
  );
  stopReloadContainerObserver.disconnect();
});

add_task(async function checkDontShowStopFromNonLocalURI() {
  let stopReloadContainer = document.getElementById("stop-reload-button");
  let stopReloadContainerObserver = new MutationObserver(
    stopReloadMutationCallback
  );

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "https://example.com",
    waitForStateStop: true,
  });
  await waitForNoAnimation(stopReloadContainer);
  stopReloadContainerObserver.observe(stopReloadContainer, {
    attributeFilter: ["animate"],
  });
  await BrowserTestUtils.loadURI(tab.linkedBrowser, "about:mozilla");
  BrowserTestUtils.removeTab(tab);

  Assert.ok(
    true,
    "Test finished: stop-reload does not animate when navigating to local URI from non-local URI"
  );
  stopReloadContainerObserver.disconnect();
});

add_task(async function checkDoShowStopOnNewTab() {
  let stopReloadContainer = document.getElementById("stop-reload-button");
  let reloadButton = document.getElementById("reload-button");
  let stopPromise = BrowserTestUtils.waitForAttribute(
    "displaystop",
    reloadButton
  );

  await waitForNoAnimation(stopReloadContainer);

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "https://example.com",
    waitForStateStop: true,
  });
  await stopPromise;
  await waitForNoAnimation(stopReloadContainer);
  BrowserTestUtils.removeTab(tab);

  info(
    "Test finished: stop-reload shows stop when navigating to non-local URI during tab opening"
  );
});

add_task(async function checkAnimateStopOnTabAfterTabFinishesOpening() {
  let stopReloadContainer = document.getElementById("stop-reload-button");

  await waitForNoAnimation(stopReloadContainer);
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    waitForStateStop: true,
  });
  await TestUtils.waitForCondition(() => {
    info(
      "Waiting for tabAnimationsInProgress to equal 0, currently " +
        gBrowser.tabAnimationsInProgress
    );
    return !gBrowser.tabAnimationsInProgress;
  });
  let animatePromise = getAnimatePromise(stopReloadContainer);
  await BrowserTestUtils.loadURI(tab.linkedBrowser, "https://example.com");
  await animatePromise;
  BrowserTestUtils.removeTab(tab);

  info(
    "Test finished: stop-reload animates when navigating to non-local URI on new tab after tab has opened"
  );
});

add_task(async function checkDoShowStopFromLocalURI() {
  let stopReloadContainer = document.getElementById("stop-reload-button");

  await waitForNoAnimation(stopReloadContainer);
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:robots",
    waitForStateStop: true,
  });
  await TestUtils.waitForCondition(() => {
    info(
      "Waiting for tabAnimationsInProgress to equal 0, currently " +
        gBrowser.tabAnimationsInProgress
    );
    return !gBrowser.tabAnimationsInProgress;
  });
  let animatePromise = getAnimatePromise(stopReloadContainer);
  await BrowserTestUtils.loadURI(tab.linkedBrowser, "https://example.com");
  await animatePromise;
  await waitForNoAnimation(stopReloadContainer);
  BrowserTestUtils.removeTab(tab);

  info(
    "Test finished: stop-reload animates when navigating to non-local URI from local URI"
  );
});
