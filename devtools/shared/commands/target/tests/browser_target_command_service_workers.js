/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API for service workers in content tabs.

const FISSION_TEST_URL = URL_ROOT_SSL + "fission_document.html";

add_task(async function () {
  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  info("Setup the test page with workers of all types");

  const tab = await addTab(FISSION_TEST_URL);

  info("Create a target list for a tab target");
  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;
  const { TYPES } = targetCommand;

  // Enable Service Worker listening.
  targetCommand.listenForServiceWorkers = true;
  await targetCommand.startListening();

  const serviceWorkerTargets = targetCommand.getAllTargets([
    TYPES.SERVICE_WORKER,
  ]);
  is(
    serviceWorkerTargets.length,
    1,
    "TargetCommmand has 1 service worker target"
  );

  info("Check that the onAvailable is done when watchTargets resolves");
  const targets = [];
  const onAvailable = async ({ targetFront }) => {
    // Wait for one second here to check that watch targets waits for
    // the onAvailable callbacks correctly.
    await wait(1000);
    targets.push(targetFront);
  };
  const onDestroyed = ({ targetFront }) =>
    targets.splice(targets.indexOf(targetFront), 1);

  await targetCommand.watchTargets({
    types: [TYPES.SERVICE_WORKER],
    onAvailable,
    onDestroyed,
  });

  // We expect onAvailable to have been called one time, for the only service
  // worker target available in the test page.
  is(targets.length, 1, "onAvailable has resolved");
  is(
    targets[0],
    serviceWorkerTargets[0],
    "onAvailable was called with the expected service worker target"
  );

  info("Unregister the worker and wait until onDestroyed is called.");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // registrationPromise is set by the test page.
    const registration = await content.wrappedJSObject.registrationPromise;
    registration.unregister();
  });
  await waitUntil(() => targets.length === 0);

  // Stop listening to avoid worker related requests
  targetCommand.destroy();

  await commands.waitForRequestsToSettle();

  await commands.destroy();
});
