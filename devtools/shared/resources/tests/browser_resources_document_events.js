/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around DOCUMENT_EVENT

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

add_task(async function() {
  await testDocumentEventResources();
  await testDocumentEventResourcesWithIgnoreExistingResources();
});

async function testDocumentEventResources() {
  info("Test ResourceWatcher for DOCUMENT_EVENT");

  // Open a test tab
  const tab = await addTab("data:text/html,Document Events");

  const listener = new ResourceListener();
  const { client, resourceWatcher, targetCommand } = await initResourceWatcher(
    tab
  );

  info(
    "Check whether the document events are fired correctly even when the document was already loaded"
  );
  const onLoadingAtInit = listener.once("dom-loading");
  const onInteractiveAtInit = listener.once("dom-interactive");
  const onCompleteAtInit = listener.once("dom-complete");
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.DOCUMENT_EVENT], {
    onAvailable: parameters => listener.dispatch(parameters),
  });
  await assertPromises(onLoadingAtInit, onInteractiveAtInit, onCompleteAtInit);
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

  info("Check whether the document events are fired correctly when reloading");
  const onLoadingAtReloaded = listener.once("dom-loading");
  const onInteractiveAtReloaded = listener.once("dom-interactive");
  const onCompleteAtReloaded = listener.once("dom-complete");
  gBrowser.reloadTab(tab);
  await assertPromises(
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

  targetCommand.destroy();
  await client.close();
}

async function testDocumentEventResourcesWithIgnoreExistingResources() {
  info("Test ignoreExistingResources option for DOCUMENT_EVENT");

  const tab = await addTab("data:text/html,Document Events");

  const { client, resourceWatcher, targetCommand } = await initResourceWatcher(
    tab
  );

  info("Check whether the existing document events will not be fired");
  const documentEvents = [];
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.DOCUMENT_EVENT], {
    onAvailable: resources => documentEvents.push(...resources),
    ignoreExistingResources: true,
  });
  is(documentEvents.length, 0, "Existing document events are not fired");

  info("Check whether the future document events are fired");
  gBrowser.reloadTab(tab);
  info("Wait for dom-loading, dom-interactive and dom-complete events");
  await waitUntil(() => documentEvents.length === 3);
  assertEvents(...documentEvents);

  targetCommand.destroy();
  await client.close();
}

async function assertPromises(onLoading, onInteractive, onComplete) {
  const loadingEvent = await onLoading;
  const interactiveEvent = await onInteractive;
  const completeEvent = await onComplete;
  assertEvents(loadingEvent, interactiveEvent, completeEvent);
}

function assertEvents(loadingEvent, interactiveEvent, completeEvent) {
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

  ok(
    loadingEvent.time < interactiveEvent.time,
    "Timestamp for interactive event is greater than loading event"
  );
  ok(
    interactiveEvent.time < completeEvent.time,
    "Timestamp for complete event is greater than interactive event"
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
