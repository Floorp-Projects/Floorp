/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around DOCUMENT_EVENT

add_task(async function () {
  await testDocumentEventResources();
  await testDocumentEventResourcesWithIgnoreExistingResources();
  await testDomCompleteWithOverloadedConsole();
  await testIframeNavigation();
  await testBfCacheNavigation();
  await testDomCompleteWithWindowStop();
  await testCrossOriginNavigation();
  await testDomCompleteWithOfflineDocument();
});

async function testDocumentEventResources() {
  info("Test ResourceCommand for DOCUMENT_EVENT");

  // Open a test tab
  const title = "DocumentEventsTitle";
  const url = `data:text/html,<title>${title}</title>Document Events`;
  const tab = await addTab(url);

  const listener = new ResourceListener();
  const { commands } = await initResourceCommand(tab);

  info(
    "Check whether the document events are fired correctly even when the document was already loaded"
  );
  const onLoadingAtInit = listener.once("dom-loading");
  const onInteractiveAtInit = listener.once("dom-interactive");
  const onCompleteAtInit = listener.once("dom-complete");
  await commands.resourceCommand.watchResources(
    [commands.resourceCommand.TYPES.DOCUMENT_EVENT],
    {
      onAvailable: parameters => listener.dispatch(parameters),
    }
  );
  await assertPromises(
    commands,
    // targetBeforeNavigation is only used when there is a will-navigate and a navigate, but there is none here
    null,
    // As we started watching on an already loaded document, and no navigation happened since we called watchResources,
    // we don't have any will-navigate event
    null,
    onLoadingAtInit,
    onInteractiveAtInit,
    onCompleteAtInit
  );
  ok(
    true,
    "Document events are fired even when the document was already loaded"
  );
  let domLoadingResource = await onLoadingAtInit;

  is(
    domLoadingResource.url,
    url,
    `resource ${domLoadingResource.name} has expected url`
  );
  is(
    domLoadingResource.title,
    undefined,
    `resource ${domLoadingResource.name} does not have a title property`
  );

  let domInteractiveResource = await onInteractiveAtInit;
  is(
    domInteractiveResource.url,
    url,
    `resource ${domInteractiveResource.name} has expected url`
  );
  is(
    domInteractiveResource.title,
    title,
    `resource ${domInteractiveResource.name} has expected title`
  );
  let domCompleteResource = await onCompleteAtInit;
  is(
    domCompleteResource.url,
    undefined,
    `resource ${domCompleteResource.name} does not have a url property`
  );
  is(
    domCompleteResource.title,
    undefined,
    `resource ${domCompleteResource.name} does not have a title property`
  );

  info("Check whether the document events are fired correctly when reloading");
  const onWillNavigate = listener.once("will-navigate");
  const onLoadingAtReloaded = listener.once("dom-loading");
  const onInteractiveAtReloaded = listener.once("dom-interactive");
  const onCompleteAtReloaded = listener.once("dom-complete");
  const targetBeforeNavigation = commands.targetCommand.targetFront;
  gBrowser.reloadTab(tab);
  await assertPromises(
    commands,
    targetBeforeNavigation,
    onWillNavigate,
    onLoadingAtReloaded,
    onInteractiveAtReloaded,
    onCompleteAtReloaded
  );
  ok(true, "Document events are fired after reloading");

  domLoadingResource = await onLoadingAtReloaded;
  is(
    domLoadingResource.url,
    url,
    `resource ${domLoadingResource.name} has expected url after reloading`
  );
  is(
    domLoadingResource.title,
    undefined,
    `resource ${domLoadingResource.name} does not have a title property after reloading`
  );

  domInteractiveResource = await onInteractiveAtInit;
  is(
    domInteractiveResource.url,
    url,
    `resource ${domInteractiveResource.name} has url property after reloading`
  );
  is(
    domInteractiveResource.title,
    title,
    `resource ${domInteractiveResource.name} has expected title after reloading`
  );
  domCompleteResource = await onCompleteAtInit;
  is(
    domCompleteResource.url,
    undefined,
    `resource ${domCompleteResource.name} does not have a url property after reloading`
  );
  is(
    domCompleteResource.title,
    undefined,
    `resource ${domCompleteResource.name} does not have a title property after reloading`
  );

  await commands.destroy();
}

async function testDocumentEventResourcesWithIgnoreExistingResources() {
  info("Test ignoreExistingResources option for DOCUMENT_EVENT");

  const tab = await addTab("data:text/html,Document Events");

  const { commands } = await initResourceCommand(tab);

  info("Check whether the existing document events will not be fired");
  const documentEvents = [];
  await commands.resourceCommand.watchResources(
    [commands.resourceCommand.TYPES.DOCUMENT_EVENT],
    {
      onAvailable: resources => documentEvents.push(...resources),
      ignoreExistingResources: true,
    }
  );
  is(documentEvents.length, 0, "Existing document events are not fired");

  info("Check whether the future document events are fired");
  const targetBeforeNavigation = commands.targetCommand.targetFront;
  gBrowser.reloadTab(tab);
  info(
    "Wait for will-navigate, dom-loading, dom-interactive and dom-complete events"
  );
  await waitFor(() => documentEvents.length === 4);
  assertEvents({ commands, targetBeforeNavigation, documentEvents });

  await commands.destroy();
}

async function testIframeNavigation() {
  info("Test iframe navigations for DOCUMENT_EVENT");

  const tab = await addTab(
    'https://example.com/document-builder.sjs?html=<iframe src="https://example.net/document-builder.sjs?html=net"></iframe>'
  );
  const secondPageUrl = "https://example.org/document-builder.sjs?html=org";

  const { commands } = await initResourceCommand(tab);

  let documentEvents = [];
  await commands.resourceCommand.watchResources(
    [commands.resourceCommand.TYPES.DOCUMENT_EVENT],
    {
      onAvailable: resources => documentEvents.push(...resources),
    }
  );
  let iframeTarget;
  if (isFissionEnabled() || isEveryFrameTargetEnabled()) {
    is(
      documentEvents.length,
      6,
      "With fission/EFT, we get two targets and two sets of events: dom-loading, dom-interactive, dom-complete"
    );
    [, iframeTarget] = await commands.targetCommand.getAllTargets([
      commands.targetCommand.TYPES.FRAME,
    ]);
    // Filter out each target events as their order to be random between the two targets
    const topTargetEvents = documentEvents.filter(
      r => r.targetFront == commands.targetCommand.targetFront
    );
    const iframeTargetEvents = documentEvents.filter(
      r => r.targetFront != commands.targetCommand.targetFront
    );
    assertEvents({
      commands,
      documentEvents: [null /* no will-navigate */, ...topTargetEvents],
    });
    assertEvents({
      commands,
      documentEvents: [null /* no will-navigate */, ...iframeTargetEvents],
      expectedTargetFront: iframeTarget,
    });
  } else {
    assertEvents({
      commands,
      documentEvents: [null /* no will-navigate */, ...documentEvents],
    });
  }

  info("Navigate the iframe to another process (if fission is enabled)");
  documentEvents = [];
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [secondPageUrl],
    function (url) {
      const iframe = content.document.querySelector("iframe");
      iframe.src = url;
    }
  );

  // We are switching to a new target only when fission is enabled...
  if (isFissionEnabled() || isEveryFrameTargetEnabled()) {
    await waitFor(() => documentEvents.length >= 3);
    is(
      documentEvents.length,
      3,
      "With fission/EFT, we switch to a new target and get: dom-loading, dom-interactive, dom-complete (but no will-navigate as that's only for the top BrowsingContext)"
    );
    const [, newIframeTarget] = await commands.targetCommand.getAllTargets([
      commands.targetCommand.TYPES.FRAME,
    ]);
    assertEvents({
      commands,
      targetBeforeNavigation: iframeTarget,
      documentEvents: [null /* no will-navigate */, ...documentEvents],
      expectedTargetFront: newIframeTarget,
      expectedNewURI: secondPageUrl,
    });
  } else {
    // Wait for some time in order to let a chance to receive some unexpected events
    await wait(250);
    is(
      documentEvents.length,
      0,
      "If fission is disabled, we navigate within the same process, we get no new target and no new resource"
    );
  }

  await commands.destroy();
}

function isBfCacheInParentEnabled() {
  return (
    Services.appinfo.sessionHistoryInParent &&
    Services.prefs.getBoolPref("fission.bfcacheInParent", false)
  );
}

async function testBfCacheNavigation() {
  info("Test bfcache navigations for DOCUMENT_EVENT");

  info("Open a first document and navigate to a second one");
  const firstLocation = "data:text/html,<title>first</title>first page";
  const secondLocation = "data:text/html,<title>second</title>second page";
  const tab = await addTab(firstLocation);
  const onLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    secondLocation
  );
  await onLoaded;

  const { commands } = await initResourceCommand(tab);

  const documentEvents = [];
  await commands.resourceCommand.watchResources(
    [commands.resourceCommand.TYPES.DOCUMENT_EVENT],
    {
      onAvailable: resources => {
        documentEvents.push(...resources);
      },
      ignoreExistingResources: true,
    }
  );
  // Wait for some time for extra safety
  await wait(250);
  is(documentEvents.length, 0, "Existing document events are not fired");

  info("Navigate back to the first page");
  const onSwitched = commands.targetCommand.once("switched-target");
  const targetBeforeNavigation = commands.targetCommand.targetFront;
  gBrowser.goBack();

  // We are switching to a new target only when fission/EFT is enabled...
  if (
    (isFissionEnabled() || isEveryFrameTargetEnabled()) &&
    isBfCacheInParentEnabled()
  ) {
    await onSwitched;
  }

  info(
    "Wait for will-navigate, dom-loading, dom-interactive and dom-complete events"
  );
  await waitFor(() => documentEvents.length >= 4);
  /* Ignore will-navigate timestamp as all other DOCUMENT_EVENTS will be set at the original load date,
     which is when we loaded from the network, and not when we loaded from bfcache */
  assertEvents({
    commands,
    targetBeforeNavigation,
    documentEvents,
    ignoreWillNavigateTimestamp: true,
  });

  // Wait for some time in order to let a chance to have duplicated dom-loading events
  await wait(250);

  is(
    documentEvents.length,
    4,
    "There is no duplicated event and only the 4 expected DOCUMENT_EVENT states"
  );
  const [willNavigateEvent, loadingEvent, interactiveEvent, completeEvent] =
    documentEvents;

  is(
    willNavigateEvent.name,
    "will-navigate",
    "The first DOCUMENT_EVENT is will-navigate"
  );
  is(
    loadingEvent.name,
    "dom-loading",
    "The second DOCUMENT_EVENT is dom-loading"
  );
  is(
    interactiveEvent.name,
    "dom-interactive",
    "The third DOCUMENT_EVENT is dom-interactive"
  );
  is(
    completeEvent.name,
    "dom-complete",
    "The fourth DOCUMENT_EVENT is dom-complete"
  );

  is(
    loadingEvent.url,
    firstLocation,
    `resource ${loadingEvent.name} has expected url after navigation back`
  );
  is(
    loadingEvent.title,
    undefined,
    `resource ${loadingEvent.name} does not have a title property after navigating back`
  );

  is(
    interactiveEvent.url,
    firstLocation,
    `resource ${interactiveEvent.name} has expected url property after navigating back`
  );
  is(
    interactiveEvent.title,
    "first",
    `resource ${interactiveEvent.name} has expected title after navigating back`
  );

  is(
    completeEvent.url,
    undefined,
    `resource ${completeEvent.name} does not have a url property after navigating back`
  );
  is(
    completeEvent.title,
    undefined,
    `resource ${completeEvent.name} does not have a title property after navigating back`
  );

  await commands.destroy();
}

async function testCrossOriginNavigation() {
  info("Test cross origin navigations for DOCUMENT_EVENT");

  const tab = await addTab("https://example.com/document-builder.sjs?html=com");

  const { commands } = await initResourceCommand(tab);

  const documentEvents = [];
  await commands.resourceCommand.watchResources(
    [commands.resourceCommand.TYPES.DOCUMENT_EVENT],
    {
      onAvailable: resources => documentEvents.push(...resources),
      ignoreExistingResources: true,
    }
  );
  // Wait for some time for extra safety
  await wait(250);
  is(documentEvents.length, 0, "Existing document events are not fired");

  info("Navigate to another process");
  const onSwitched = commands.targetCommand.once("switched-target");
  const netUrl =
    "https://example.net/document-builder.sjs?html=<head><title>titleNet</title></head>net";
  const onLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  const targetBeforeNavigation = commands.targetCommand.targetFront;
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, netUrl);
  await onLoaded;

  // We are switching to a new target only when fission is enabled...
  if (isFissionEnabled() || isEveryFrameTargetEnabled()) {
    await onSwitched;
  }

  info(
    "Wait for will-navigate, dom-loading, dom-interactive and dom-complete events"
  );
  await waitFor(() => documentEvents.length >= 4);
  assertEvents({ commands, targetBeforeNavigation, documentEvents });

  // Wait for some time in order to let a chance to have duplicated dom-loading events
  await wait(250);

  is(
    documentEvents.length,
    4,
    "There is no duplicated event and only the 4 expected DOCUMENT_EVENT states"
  );
  const [willNavigateEvent, loadingEvent, interactiveEvent, completeEvent] =
    documentEvents;

  is(
    willNavigateEvent.name,
    "will-navigate",
    "The first DOCUMENT_EVENT is will-navigate"
  );
  is(
    loadingEvent.name,
    "dom-loading",
    "The second DOCUMENT_EVENT is dom-loading"
  );
  is(
    interactiveEvent.name,
    "dom-interactive",
    "The third DOCUMENT_EVENT is dom-interactive"
  );
  is(
    completeEvent.name,
    "dom-complete",
    "The fourth DOCUMENT_EVENT is dom-complete"
  );

  is(
    loadingEvent.url,
    encodeURI(netUrl),
    `resource ${loadingEvent.name} has expected url after reloading`
  );
  is(
    loadingEvent.title,
    undefined,
    `resource ${loadingEvent.name} does not have a title property after reloading`
  );

  is(
    interactiveEvent.url,
    encodeURI(netUrl),
    `resource ${interactiveEvent.name} has expected url property after reloading`
  );
  is(
    interactiveEvent.title,
    "titleNet",
    `resource ${interactiveEvent.name} has expected title after reloading`
  );

  is(
    completeEvent.url,
    undefined,
    `resource ${completeEvent.name} does not have a url property after reloading`
  );
  is(
    completeEvent.title,
    undefined,
    `resource ${completeEvent.name} does not have a title property after reloading`
  );

  await commands.destroy();
}

async function testDomCompleteWithOverloadedConsole() {
  info("Test dom-complete with an overloaded console object");

  const tab = await addTab(
    "data:text/html,<script>window.console = {};</script>"
  );

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  info("Check that all DOCUMENT_EVENTS are fired for the already loaded page");
  const documentEvents = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.DOCUMENT_EVENT], {
    onAvailable: resources => documentEvents.push(...resources),
  });
  is(documentEvents.length, 3, "Existing document events are fired");

  const domComplete = documentEvents[2];
  is(domComplete.name, "dom-complete", "the last resource is the dom-complete");
  is(
    domComplete.hasNativeConsoleAPI,
    false,
    "the console object is reported to be overloaded"
  );

  targetCommand.destroy();
  await client.close();
}

async function testDomCompleteWithWindowStop() {
  info("Test dom-complete with a page calling window.stop()");

  const tab = await addTab("data:text/html,foo");

  const { commands, client, resourceCommand, targetCommand } =
    await initResourceCommand(tab);

  info("Check that all DOCUMENT_EVENTS are fired for the already loaded page");
  let documentEvents = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.DOCUMENT_EVENT], {
    onAvailable: resources => documentEvents.push(...resources),
  });
  is(documentEvents.length, 3, "Existing document events are fired");
  documentEvents = [];

  const html = `<!DOCTYPE html><html>
  <head>
    <title>stopped page</title>
    <script>window.stop();</script>
  </head>
  <body>Page content that shouldn't be displayed</body>
</html>`;
  const secondLocation = "data:text/html," + encodeURIComponent(html);
  const targetBeforeNavigation = commands.targetCommand.targetFront;
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    secondLocation
  );
  info(
    "Wait for will-navigate, dom-loading, dom-interactive and dom-complete events"
  );
  await waitFor(() => documentEvents.length === 4);

  assertEvents({ commands, targetBeforeNavigation, documentEvents });

  targetCommand.destroy();
  await client.close();
}

async function testDomCompleteWithOfflineDocument() {
  info("Test dom-complete with an offline page");

  const tab = await addTab(`${URL_ROOT_SSL}empty.html`);

  const { commands, client, resourceCommand, targetCommand } =
    await initResourceCommand(tab);

  info("Check that all DOCUMENT_EVENTS are fired for the already loaded page");
  let documentEvents = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.DOCUMENT_EVENT], {
    onAvailable: resources => documentEvents.push(...resources),
  });
  is(documentEvents.length, 3, "Existing document events are fired");
  documentEvents = [];

  const targetBeforeNavigation = commands.targetCommand.targetFront;
  tab.linkedBrowser.browsingContext.forceOffline = true;
  gBrowser.reloadTab(tab);

  // The offline mode may break Document Event Watcher and we would miss some of the expected events
  info(
    "Wait for will-navigate, dom-loading, dom-interactive and dom-complete events"
  );
  await waitFor(() => documentEvents.length === 4);

  // Only will-navigate will have a valid timestamp, as the page is failing loading
  // and we get the offline notice page, the other events will be set to 0
  assertEvents({
    commands,
    targetBeforeNavigation,
    documentEvents,
    ignoreAllTimestamps: true,
  });

  targetCommand.destroy();
  await client.close();
}

async function assertPromises(
  commands,
  targetBeforeNavigation,
  onWillNavigate,
  onLoading,
  onInteractive,
  onComplete
) {
  const willNavigateEvent = await onWillNavigate;
  const loadingEvent = await onLoading;
  const interactiveEvent = await onInteractive;
  const completeEvent = await onComplete;
  assertEvents({
    commands,
    targetBeforeNavigation,
    documentEvents: [
      willNavigateEvent,
      loadingEvent,
      interactiveEvent,
      completeEvent,
    ],
  });
}

function assertEvents({
  commands,
  targetBeforeNavigation,
  documentEvents,
  expectedTargetFront = commands.targetCommand.targetFront,
  expectedNewURI = gBrowser.selectedBrowser.currentURI.spec,
  ignoreWillNavigateTimestamp = false,
  ignoreAllTimestamps = false,
}) {
  const [willNavigateEvent, loadingEvent, interactiveEvent, completeEvent] =
    documentEvents;
  if (willNavigateEvent) {
    is(willNavigateEvent.name, "will-navigate", "Received the will-navigate");
    is(
      willNavigateEvent.newURI,
      expectedNewURI,
      "will-navigate newURI is set to the current tab new location"
    );
  }
  is(
    loadingEvent.name,
    "dom-loading",
    "loading received in the exepected order"
  );
  is(
    interactiveEvent.name,
    "dom-interactive",
    "interactive received in the expected order"
  );
  is(completeEvent.name, "dom-complete", "complete received last");

  if (willNavigateEvent) {
    is(
      typeof willNavigateEvent.time,
      "number",
      `Type of time attribute for will-navigate event is correct (${willNavigateEvent.time})`
    );
  }
  is(
    typeof loadingEvent.time,
    "number",
    `Type of time attribute for loading event is correct (${loadingEvent.time})`
  );
  is(
    typeof interactiveEvent.time,
    "number",
    `Type of time attribute for interactive event is correct (${interactiveEvent.time})`
  );
  is(
    typeof completeEvent.time,
    "number",
    `Type of time attribute for complete event is correct (${completeEvent.time})`
  );

  // In case of errors the timestamps may be set to 0.
  if (!ignoreAllTimestamps) {
    if (willNavigateEvent && !ignoreWillNavigateTimestamp) {
      Assert.lessOrEqual(
        willNavigateEvent.time,
        loadingEvent.time,
        `Timestamp for dom-loading event is greater than will-navigate event (${willNavigateEvent.time} <= ${loadingEvent.time})`
      );
    }
    Assert.lessOrEqual(
      loadingEvent.time,
      interactiveEvent.time,
      `Timestamp for interactive event is greater than loading event (${loadingEvent.time} <= ${interactiveEvent.time})`
    );
    Assert.lessOrEqual(
      interactiveEvent.time,
      completeEvent.time,
      `Timestamp for complete event is greater than interactive event (${interactiveEvent.time} <= ${completeEvent.time}).`
    );
  }

  if (willNavigateEvent) {
    // If we switched to a new target, this target will be different from currentTargetFront.
    // This only happen if we navigate to another process or if server target switching is enabled.
    is(
      willNavigateEvent.targetFront,
      targetBeforeNavigation,
      "will-navigate target was the one before the navigation"
    );
  }
  is(
    loadingEvent.targetFront,
    expectedTargetFront,
    "loading target is the expected one"
  );
  is(
    interactiveEvent.targetFront,
    expectedTargetFront,
    "interactive target is the expected one"
  );
  is(
    completeEvent.targetFront,
    expectedTargetFront,
    "complete target is the expected one"
  );

  is(
    completeEvent.hasNativeConsoleAPI,
    true,
    "None of the tests (except the dedicated one) overload the console object"
  );
}

class ResourceListener {
  _listeners = new Map();

  dispatch(resources) {
    for (const resource of resources) {
      const resolve = this._listeners.get(resource.name);
      if (resolve) {
        resolve(resource);
        this._listeners.delete(resource.name);
      }
    }
  }

  once(resourceName) {
    return new Promise(r => this._listeners.set(resourceName, r));
  }
}
