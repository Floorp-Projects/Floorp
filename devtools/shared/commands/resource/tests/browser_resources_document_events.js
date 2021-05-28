/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around DOCUMENT_EVENT

add_task(async function() {
  await testDocumentEventResources();
  await testDocumentEventResourcesWithIgnoreExistingResources();

  // Enable server side target switching for next test
  // as the regression it tracks only occurs with server side target switching enabled
  await pushPref("devtools.target-switching.server.enabled", true);
  await testCrossOriginNavigation();
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
    domLoadingResource.shouldBeIgnoredAsRedundantWithTargetAvailable,
    true,
    "shouldBeIgnoredAsRedundantWithTargetAvailable is true for already loaded page"
  );

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
    undefined,
    `resource ${domInteractiveResource.name} does not have a url property`
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
    domLoadingResource.shouldBeIgnoredAsRedundantWithTargetAvailable,
    undefined,
    "shouldBeIgnoredAsRedundantWithTargetAvailable is not set after reloading"
  );

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
    undefined,
    `resource ${domInteractiveResource.name} does not have a url property after reloading`
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
  await waitUntil(() => documentEvents.length === 4);
  assertEvents(commands, targetBeforeNavigation, ...documentEvents);

  await commands.destroy();
}

async function testCrossOriginNavigation() {
  info("Test cross origin navigations for DOCUMENT_EVENT");

  const tab = await addTab("http://example.com/document-builder.sjs?html=com");

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
  await wait(1000);
  is(documentEvents.length, 0, "Existing document events are not fired");

  info("Navigate to another process");
  const onSwitched = commands.targetCommand.once("switched-target");
  const netUrl =
    "http://example.net/document-builder.sjs?html=<head><title>titleNet</title></head>net";
  const onLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  const targetBeforeNavigation = commands.targetCommand.targetFront;
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, netUrl);
  await onLoaded;

  // We are switching to a new target only when fission is enabled...
  if (isFissionEnabled()) {
    await onSwitched;
  }

  info(
    "Wait for will-navigate, dom-loading, dom-interactive and dom-complete events"
  );
  await waitUntil(() => documentEvents.length >= 4);
  assertEvents(commands, targetBeforeNavigation, ...documentEvents);

  // Wait for some time in order to let a chance to have duplicated dom-loading events
  await wait(1000);

  is(
    documentEvents.length,
    4,
    "There is no duplicated event and only the 4 expected DOCUMENT_EVENT states"
  );
  const [
    willNavigateEvent,
    loadingEvent,
    interactiveEvent,
    completeEvent,
  ] = documentEvents;

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

  // followWindowGlobalLifeCycle will be true when enabling server side target switching,
  // even when fission is off.
  if (
    isFissionEnabled() ||
    commands.targetCommand.targetFront.targetForm.followWindowGlobalLifeCycle
  ) {
    is(
      loadingEvent.shouldBeIgnoredAsRedundantWithTargetAvailable,
      true,
      "shouldBeIgnoredAsRedundantWithTargetAvailable is true for the new target which follows the WindowGlobal lifecycle"
    );
  } else {
    is(
      loadingEvent.shouldBeIgnoredAsRedundantWithTargetAvailable,
      undefined,
      "shouldBeIgnoredAsRedundantWithTargetAvailable is undefined if fission is disabled and we keep the same target"
    );
  }

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
    undefined,
    `resource ${interactiveEvent.name} does not have a url property after reloading`
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
  assertEvents(
    commands,
    targetBeforeNavigation,
    willNavigateEvent,
    loadingEvent,
    interactiveEvent,
    completeEvent
  );
}

function assertEvents(
  commands,
  targetBeforeNavigation,
  willNavigateEvent,
  loadingEvent,
  interactiveEvent,
  completeEvent
) {
  if (willNavigateEvent) {
    is(willNavigateEvent.name, "will-navigate", "Received the will-navigate");
    is(
      willNavigateEvent.newURI,
      gBrowser.selectedBrowser.currentURI.spec,
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
      "Type of time attribute for will-navigate event is correct"
    );
  }
  is(
    typeof loadingEvent.time,
    "number",
    "Type of time attribute for loading event is correct"
  );
  is(
    typeof interactiveEvent.time,
    "number",
    "Type of time attribute for interactive event is correct"
  );
  is(
    typeof completeEvent.time,
    "number",
    "Type of time attribute for complete event is correct"
  );

  if (willNavigateEvent) {
    ok(
      willNavigateEvent.time < loadingEvent.time,
      "Timestamp for dom-loading event is greater than will-navigate event"
    );
  }
  ok(
    loadingEvent.time < interactiveEvent.time,
    "Timestamp for interactive event is greater than loading event"
  );
  ok(
    interactiveEvent.time < completeEvent.time,
    "Timestamp for complete event is greater than interactive event"
  );

  const currentTargetFront = commands.targetCommand.targetFront;
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
    currentTargetFront,
    "loading target is the current target"
  );
  is(
    interactiveEvent.targetFront,
    currentTargetFront,
    "interactive target is the current target"
  );
  is(
    completeEvent.targetFront,
    currentTargetFront,
    "complete target is the current target"
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
