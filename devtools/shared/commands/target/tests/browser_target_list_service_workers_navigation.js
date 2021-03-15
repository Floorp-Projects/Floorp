/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API for service workers when navigating in content tabs.
// When the top level target navigates, we manually call onTargetAvailable for
// service workers which now match the page domain. We assert that the callbacks
// will be called the expected number of times here.

const COM_PAGE_URL = URL_ROOT_SSL + "test_sw_page.html";
const COM_WORKER_URL = URL_ROOT_SSL + "test_sw_page_worker.js";
const ORG_PAGE_URL = URL_ROOT_ORG_SSL + "test_sw_page.html";
const ORG_WORKER_URL = URL_ROOT_ORG_SSL + "test_sw_page_worker.js";

/**
 * This test will navigate between two pages, both controlled by different
 * service workers.
 *
 * The steps will be:
 * - navigate to .com page
 * - create target list
 * - navigate to .org page
 * - reload .org page
 * - unregister .org worker
 * - navigate back to .com page
 * - unregister .com worker
 *
 * First we test this with destroyServiceWorkersOnNavigation = false.
 * In this case we expect the following calls:
 * - navigate to .com page
 * - create target list
 *   - onAvailable should be called for the .com worker
 * - navigate to .org page
 *   - onAvailable should be called for the .org worker
 * - reload .org page
 *   - nothing should happen
 * - unregister .org worker
 *   - onDestroyed should be called for the .org worker
 * - navigate back to .com page
 *   - nothing should happen
 * - unregister .com worker
 *   - onDestroyed should be called for the .com worker
 */
add_task(async function test_NavigationBetweenTwoDomains_NoDestroy() {
  const { client, mainRoot } = await setupServiceWorkerNavigationTest();

  const tab = await addTab(COM_PAGE_URL);

  const { hooks, targetList } = await watchServiceWorkerTargets({
    mainRoot,
    tab,
    destroyServiceWorkersOnNavigation: false,
  });

  // We expect onAvailable to have been called one time, for the only service
  // worker target available in the test page.
  await checkHooks(hooks, {
    available: 1,
    destroyed: 0,
    targets: [COM_WORKER_URL],
  });

  info("Go to .org page, wait for onAvailable to be called");
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, ORG_PAGE_URL);
  await checkHooks(hooks, {
    available: 2,
    destroyed: 0,
    targets: [COM_WORKER_URL, ORG_WORKER_URL],
  });

  info("Reload .org page, onAvailable and onDestroyed should not be called");
  const reloaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.reloadTab(gBrowser.selectedTab);
  await reloaded;
  await checkHooks(hooks, {
    available: 2,
    destroyed: 0,
    targets: [COM_WORKER_URL, ORG_WORKER_URL],
  });

  info("Unregister .org service worker and wait until onDestroyed is called.");
  await unregisterServiceWorker(tab, ORG_PAGE_URL);
  await checkHooks(hooks, {
    available: 2,
    destroyed: 1,
    targets: [COM_WORKER_URL],
  });

  info("Go back to page 1");
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, COM_PAGE_URL);
  await checkHooks(hooks, {
    available: 2,
    destroyed: 1,
    targets: [COM_WORKER_URL],
  });

  info("Unregister .com service worker and wait until onDestroyed is called.");
  await unregisterServiceWorker(tab, COM_PAGE_URL);
  await checkHooks(hooks, { available: 2, destroyed: 2, targets: [] });

  // Stop listening to avoid worker related requests
  targetList.destroy();

  await client.waitForRequestsToSettle();
  await client.close();
  await removeTab(tab);
});

/**
 * Same scenario as test_NavigationBetweenTwoDomains_NoDestroy, but this time
 * with destroyServiceWorkersOnNavigation set to true.
 *
 * In this case we expect the following calls:
 * - navigate to .com page
 * - create target list
 *   - onAvailable should be called for the .com worker
 * - navigate to .org page
 *   - onDestroyed should be called for the .com worker
 *   - onAvailable should be called for the .org worker
 * - reload .org page
 *   - onDestroyed & onAvailable should be called for the .org worker
 * - unregister .org worker
 *   - onDestroyed should be called for the .org worker
 * - navigate back to .com page
 *   - onAvailable should be called for the .com worker
 * - unregister .com worker
 *   - onDestroyed should be called for the .com worker
 */
add_task(async function test_NavigationBetweenTwoDomains_WithDestroy() {
  const { client, mainRoot } = await setupServiceWorkerNavigationTest();

  const tab = await addTab(COM_PAGE_URL);

  const { hooks, targetList } = await watchServiceWorkerTargets({
    mainRoot,
    tab,
    destroyServiceWorkersOnNavigation: true,
  });

  // We expect onAvailable to have been called one time, for the only service
  // worker target available in the test page.
  await checkHooks(hooks, {
    available: 1,
    destroyed: 0,
    targets: [COM_WORKER_URL],
  });

  info("Go to .org page, wait for onAvailable to be called");
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, ORG_PAGE_URL);
  await checkHooks(hooks, {
    available: 2,
    destroyed: 1,
    targets: [ORG_WORKER_URL],
  });

  info("Reload .org page, onAvailable and onDestroyed should be called");
  gBrowser.reloadTab(gBrowser.selectedTab);
  await checkHooks(hooks, {
    available: 3,
    destroyed: 2,
    targets: [ORG_WORKER_URL],
  });

  info("Unregister .org service worker and wait until onDestroyed is called.");
  await unregisterServiceWorker(tab, ORG_PAGE_URL);
  await checkHooks(hooks, { available: 3, destroyed: 3, targets: [] });

  info("Go back to page 1, wait for onDestroyed and onAvailable to be called");
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, COM_PAGE_URL);
  await checkHooks(hooks, {
    available: 4,
    destroyed: 3,
    targets: [COM_WORKER_URL],
  });

  info("Unregister .com service worker and wait until onDestroyed is called.");
  await unregisterServiceWorker(tab, COM_PAGE_URL);
  await checkHooks(hooks, { available: 4, destroyed: 4, targets: [] });

  // Stop listening to avoid worker related requests
  targetList.destroy();

  await client.waitForRequestsToSettle();
  await client.close();
  await removeTab(tab);
});

/**
 * In this test we load a service worker in a page prior to starting the
 * TargetCommand. We start the target list on another page, and then we go back to
 * the first page. We want to check that we are correctly notified about the
 * worker that was spawned before TargetCommand.
 *
 * Steps:
 * - navigate to .com page
 * - navigate to .org page
 * - create target list
 * - unregister .org worker
 * - navigate back to .com page
 * - unregister .com worker
 *
 * The expected calls are the same whether destroyServiceWorkersOnNavigation is
 * true or false.
 *
 * Expected calls:
 * - navigate to .com page
 * - navigate to .org page
 * - create target list
 *   - onAvailable is called for the .org worker
 * - unregister .org worker
 *   - onDestroyed is called for the .org worker
 * - navigate back to .com page
 *   - onAvailable is called for the .com worker
 * - unregister .com worker
 *   - onDestroyed is called for the .com worker
 */
add_task(async function test_NavigationToPageWithExistingWorker_NoDestroy() {
  await testNavigationToPageWithExistingWorker({
    destroyServiceWorkersOnNavigation: false,
  });
});

add_task(async function test_NavigationToPageWithExistingWorker_WithDestroy() {
  await testNavigationToPageWithExistingWorker({
    destroyServiceWorkersOnNavigation: true,
  });
});

async function testNavigationToPageWithExistingWorker({
  destroyServiceWorkersOnNavigation,
}) {
  const { client, mainRoot } = await setupServiceWorkerNavigationTest();

  const tab = await addTab(COM_PAGE_URL);

  info("Wait until the service worker registration is registered");
  await waitForRegistrationReady(tab, COM_PAGE_URL);

  info("Navigate to another page");
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, ORG_PAGE_URL);

  // Avoid TV failures, where target list still starts thinking that the
  // current domain is .com .
  info("Wait until we have fully navigated to the .org page");
  await waitForRegistrationReady(tab, ORG_PAGE_URL);

  const { hooks, targetList } = await watchServiceWorkerTargets({
    mainRoot,
    tab,
    destroyServiceWorkersOnNavigation,
  });

  // We expect onAvailable to have been called one time, for the only service
  // worker target available in the test page.
  await checkHooks(hooks, {
    available: 1,
    destroyed: 0,
    targets: [ORG_WORKER_URL],
  });

  info("Unregister .org service worker and wait until onDestroyed is called.");
  await unregisterServiceWorker(tab, ORG_PAGE_URL);
  await checkHooks(hooks, { available: 1, destroyed: 1, targets: [] });

  info("Go back .com page, wait for onAvailable to be called");
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, COM_PAGE_URL);
  await checkHooks(hooks, {
    available: 2,
    destroyed: 1,
    targets: [COM_WORKER_URL],
  });

  info("Unregister .com service worker and wait until onDestroyed is called.");
  await unregisterServiceWorker(tab, COM_PAGE_URL);
  await checkHooks(hooks, { available: 2, destroyed: 2, targets: [] });

  // Stop listening to avoid worker related requests
  targetList.destroy();

  await client.waitForRequestsToSettle();
  await client.close();
  await removeTab(tab);
}

async function setupServiceWorkerNavigationTest() {
  // Enabled devtools.browsertoolbox.fission to listen to all target types.
  await pushPref("devtools.browsertoolbox.fission", true);

  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  info("Setup the test page with workers of all types");
  const client = await createLocalClient();
  const mainRoot = client.mainRoot;
  return { client, mainRoot };
}

async function watchServiceWorkerTargets({
  destroyServiceWorkersOnNavigation,
  mainRoot,
  tab,
}) {
  info("Create a target list for a tab target");
  const descriptor = await mainRoot.getTab({ tab });
  const commands = await descriptor.getCommands();
  const targetList = commands.targetCommand;

  // Enable Service Worker listening.
  targetList.listenForServiceWorkers = true;
  info(
    "Set targetList.destroyServiceWorkersOnNavigation to " +
      destroyServiceWorkersOnNavigation
  );
  targetList.destroyServiceWorkersOnNavigation = destroyServiceWorkersOnNavigation;
  await targetList.startListening();

  // Setup onAvailable & onDestroyed callbacks so that we can check how many
  // times they are called and with which targetFront.
  const hooks = {
    availableCount: 0,
    destroyedCount: 0,
    targets: [],
  };

  const onAvailable = async ({ targetFront }) => {
    hooks.availableCount++;
    hooks.targets.push(targetFront);
  };

  const onDestroyed = ({ targetFront }) => {
    hooks.destroyedCount++;
    hooks.targets.splice(hooks.targets.indexOf(targetFront), 1);
  };

  await targetList.watchTargets(
    [targetList.TYPES.SERVICE_WORKER],
    onAvailable,
    onDestroyed
  );

  return { hooks, targetList };
}

async function unregisterServiceWorker(tab, expectedPageUrl) {
  await waitForRegistrationReady(tab, expectedPageUrl);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // registrationPromise is set by the test page.
    const registration = await content.wrappedJSObject.registrationPromise;
    registration.unregister();
  });
}

/**
 * Wait until the expected URL is loaded and win.registration has resolved.
 */
async function waitForRegistrationReady(tab, expectedPageUrl) {
  await asyncWaitUntil(() =>
    SpecialPowers.spawn(tab.linkedBrowser, [expectedPageUrl], function(_url) {
      try {
        const win = content.wrappedJSObject;
        const isExpectedUrl = win.location.href === _url;
        const hasRegistration = !!win.registrationPromise;
        return isExpectedUrl && hasRegistration;
      } catch (e) {
        return false;
      }
    })
  );
}

/**
 * Assert helper for the `hooks` object, updated by the onAvailable and
 * onDestroyed callbacks. Assert that the callbacks have been called the
 * expected number of times, with the expected targets.
 */
async function checkHooks(hooks, { available, destroyed, targets }) {
  info(`Wait for availableCount=${available} and destroyedCount=${destroyed}`);
  await waitUntil(
    () => hooks.availableCount == available && hooks.destroyedCount == destroyed
  );
  is(hooks.availableCount, available, "onAvailable was called as expected");
  is(hooks.destroyedCount, destroyed, "onDestroyed was called as expected");

  is(hooks.targets.length, targets.length, "Expected number of targets");
  targets.forEach((url, i) => {
    is(hooks.targets[i].url, url, `SW target ${i} has the expected url`);
  });
}
