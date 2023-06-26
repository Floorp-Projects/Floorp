/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Document the basics of DevTools backend via Fronts in a test.
 */

"use strict";

const TEST_URL = "data:text/html,new-tab";

add_task(async () => {
  // Allow logging all RDP packets
  await pushPref("devtools.debugger.log", true);
  // Really all of them
  await pushPref("devtools.debugger.log.verbose", true);

  // Instantiate a DevTools server
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  // Instantiate a client connected to this server
  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);

  // This will trigger some handshake with the server
  await client.connect();

  // You need to call listTabs once to retrieve the existing list of Tab Descriptor actors...
  const tabs = await client.mainRoot.listTabs();

  // ... which will let you receive the 'tabListChanged' event.
  // This is an empty RDP packet, you need to re-call listTabs to get the full new updated list of actors.
  const onTabListUpdated = client.mainRoot.once("tabListChanged");

  // Open a new tab.
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  await onTabListUpdated;

  // The new list of Tab descriptors should contain the newly opened tab
  const newTabs = await client.mainRoot.listTabs();
  is(newTabs.length, tabs.length + 1);

  const tabDescriptorActor = newTabs.pop();
  is(tabDescriptorActor.url, TEST_URL);

  // Query the Tab Descriptor actor to retrieve its related Watcher actor.
  // Each Descriptor actor has a dedicated watcher which will be scoped to the context of the descriptor.
  // Here the watcher will focus on the related tab.
  const watcherActor = await tabDescriptorActor.getWatcher();

  // The call to Watcher Actor's watchTargets will emit target-available-form RDP events.
  // One per available target. It will emit one for each immediatly available target,
  // but also for any available later. That, until you call unwatchTarget method.
  //
  // Here I'm listening to "target-available" to get a Front instance, which helps call RDP methods.
  // But this isn't an RDP event. This is a frontend-only thing.
  const onTopTargetAvailable = watcherActor.once("target-available");

  // watchTargets accepts "frame", "process" and "worker"
  // When debugging a web page you want to listen to frame and worker targets.
  // "frame" would better be named "WindowGlobal" as it will notify you about all the WindowGlobal of the page.
  // Each top level documents and any iframe documents will have a related WindowGlobal,
  // if any of these documents navigate, a new WindowGlobal will be instantiated.
  // If you care about workers, listen to worker targets as well.
  await watcherActor.watchTargets("frame");

  // This is a trivial example so we have a unique WindowGlobal target for the top level document
  const topTarget = await onTopTargetAvailable;
  is(topTarget.url, TEST_URL);

  // Similarly to watchTarget, the next call to watchResources will emit new resources right away as well as later.
  const onConsoleMessages = topTarget.once("resource-available-form");

  // If you want to observe anything, you have to use Watcher Actor's watchrResources API.
  // The list of all available resources is here:
  // https://searchfox.org/mozilla-central/source/devtools/server/actors/resources/index.js#9
  // And you might have a look at each ResourceWatcher subclass to learn more about the fields exposed by each resource type:
  // https://searchfox.org/mozilla-central/source/devtools/server/actors/resources
  await watcherActor.watchResources(["console-message"]);

  // You may use many useful actors on each target actor, like console, thread, ...
  // You can get the full list of available actors in:
  // https://searchfox.org/mozilla-central/source/devtools/server/actors/utils/actor-registry.js#176
  // And then look into the mentioned path for implementation.
  const webConsoleActor = await topTarget.getFront("console");

  // Call the Console API in order to force emitting a console-message resource
  await webConsoleActor.evaluateJSAsync({ text: "console.log('42')" });

  // Wait for the related console-message resource
  const resources = await onConsoleMessages;

  // Note that resource-available-form comes with a "resources" attribute which is an array of resources
  // which may contain various resource types.
  is(resources[0].message.arguments[0], "42");

  await client.close();
});
