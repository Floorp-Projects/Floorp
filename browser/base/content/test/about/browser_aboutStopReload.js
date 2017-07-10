async function waitForNoAnimation(elt) {
  return BrowserTestUtils.waitForCondition(() => !elt.hasAttribute("animate"));
}

async function getAnimatePromise(elt) {
  return BrowserTestUtils.waitForAttribute("animate", elt)
    .then(() => Assert.ok(true, `${elt.id} should animate`));
}

function stopReloadMutationCallback() {
  Assert.ok(false, "stop-reload's animate attribute should not have been mutated");
}

add_task(async function checkDontShowStopOnNewTab() {
  let stopReloadContainer = document.getElementById("stop-reload-button");
  let stopReloadContainerObserver = new MutationObserver(stopReloadMutationCallback);

  await waitForNoAnimation(stopReloadContainer);
  stopReloadContainerObserver.observe(stopReloadContainer, { attributeFilter: ["animate"]});
  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
                                                        opening: "about:home",
                                                        waitForStateStop: true});
  await BrowserTestUtils.removeTab(tab);

  Assert.ok(true, "Test finished: stop-reload does not animate when navigating to local URI on new tab");
  stopReloadContainerObserver.disconnect();
});

add_task(async function checkDontShowStopFromLocalURI() {
  let stopReloadContainer = document.getElementById("stop-reload-button");
  let stopReloadContainerObserver = new MutationObserver(stopReloadMutationCallback);

  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
                                                        opening: "about:home",
                                                        waitForStateStop: true});
  await waitForNoAnimation(stopReloadContainer);
  stopReloadContainerObserver.observe(stopReloadContainer, { attributeFilter: ["animate"]});
  await BrowserTestUtils.loadURI(tab.linkedBrowser, "about:mozilla");
  await BrowserTestUtils.removeTab(tab);

  Assert.ok(true, "Test finished: stop-reload does not animate when navigating between local URIs");
  stopReloadContainerObserver.disconnect();
});

add_task(async function checkDontShowStopFromNonLocalURI() {
  let stopReloadContainer = document.getElementById("stop-reload-button");
  let stopReloadContainerObserver = new MutationObserver(stopReloadMutationCallback);

  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
                                                        opening: "https://example.com",
                                                        waitForStateStop: true});
  await waitForNoAnimation(stopReloadContainer);
  stopReloadContainerObserver.observe(stopReloadContainer, { attributeFilter: ["animate"]});
  await BrowserTestUtils.loadURI(tab.linkedBrowser, "about:mozilla");
  await BrowserTestUtils.removeTab(tab);

  Assert.ok(true, "Test finished: stop-reload does not animate when navigating to local URI from non-local URI");
  stopReloadContainerObserver.disconnect();
});

add_task(async function checkDoShowStopOnNewTab() {
  let stopReloadContainer = document.getElementById("stop-reload-button");
  let animatePromise = getAnimatePromise(stopReloadContainer);

  await waitForNoAnimation(stopReloadContainer);
  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
                                                        opening: "https://example.com",
                                                        waitForStateStop: true});
  await animatePromise;
  await waitForNoAnimation(stopReloadContainer);
  await BrowserTestUtils.removeTab(tab);

  info("Test finished: stop-reload animates when navigating to non-local URI on new tab");
});

add_task(async function checkDoShowStopFromLocalURI() {
  let stopReloadContainer = document.getElementById("stop-reload-button");

  await waitForNoAnimation(stopReloadContainer);
  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
                                                        opening: "about:home",
                                                        waitForStateStop: true});
  let animatePromise = getAnimatePromise(stopReloadContainer);
  BrowserTestUtils.loadURI(tab.linkedBrowser, "https://example.com");
  await animatePromise;
  await waitForNoAnimation(stopReloadContainer);
  await BrowserTestUtils.removeTab(tab);

  info("Test finished: stop-reload animates when navigating local URI from non-local URI");
});
