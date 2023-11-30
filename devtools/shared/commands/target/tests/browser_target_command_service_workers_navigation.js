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
 *   -> onAvailable should be called for the .com worker
 * - navigate to .org page
 *   -> onAvailable should be called for the .org worker
 * - reload .org page
 *   -> nothing should happen
 * - unregister .org worker
 *   -> onDestroyed should be called for the .org worker
 * - navigate back to .com page
 *   -> nothing should happen
 * - unregister .com worker
 *   -> onDestroyed should be called for the .com worker
 */
add_task(async function test_NavigationBetweenTwoDomains_NoDestroy() {
  await setupServiceWorkerNavigationTest();

  const tab = await addTab(COM_PAGE_URL);

  const { hooks, commands, targetCommand } = await watchServiceWorkerTargets(
    tab
  );

  // We expect onAvailable to have been called one time, for the only service
  // worker target available in the test page.
  await checkHooks(hooks, {
    available: 1,
    destroyed: 0,
    targets: [COM_WORKER_URL],
  });

  info("Go to .org page, wait for onAvailable to be called");
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    ORG_PAGE_URL
  );
  await checkHooks(hooks, {
    available: 2,
    destroyed: 0,
    targets: [COM_WORKER_URL, ORG_WORKER_URL],
  });

  info("Reload .org page, onAvailable and onDestroyed should not be called");
  await BrowserTestUtils.reloadTab(gBrowser.selectedTab);
  await checkHooks(hooks, {
    available: 2,
    destroyed: 0,
    targets: [COM_WORKER_URL, ORG_WORKER_URL],
  });

  info("Unregister .org service worker and wait until onDestroyed is called.");
  await unregisterServiceWorker(ORG_WORKER_URL);
  await checkHooks(hooks, {
    available: 2,
    destroyed: 1,
    targets: [COM_WORKER_URL],
  });

  info("Go back to .com page");
  const onBrowserLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    COM_PAGE_URL
  );
  await onBrowserLoaded;
  await checkHooks(hooks, {
    available: 2,
    destroyed: 1,
    targets: [COM_WORKER_URL],
  });

  info("Unregister .com service worker and wait until onDestroyed is called.");
  await unregisterServiceWorker(COM_WORKER_URL);
  await checkHooks(hooks, {
    available: 2,
    destroyed: 2,
    targets: [],
  });

  // Stop listening to avoid worker related requests
  targetCommand.destroy();

  await commands.waitForRequestsToSettle();
  await commands.destroy();
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
 *   -> onAvailable is called for the .org worker
 * - unregister .org worker
 *   -> onDestroyed is called for the .org worker
 * - navigate back to .com page
 *   -> onAvailable is called for the .com worker
 * - unregister .com worker
 *   -> onDestroyed is called for the .com worker
 */
add_task(async function test_NavigationToPageWithExistingWorker() {
  await setupServiceWorkerNavigationTest();

  const tab = await addTab(COM_PAGE_URL);

  info("Wait until the service worker registration is registered");
  await waitForRegistrationReady(tab, COM_PAGE_URL, COM_WORKER_URL);

  info("Navigate to another page");
  let onBrowserLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    ORG_PAGE_URL
  );

  // Avoid TV failures, where target list still starts thinking that the
  // current domain is .com .
  info("Wait until we have fully navigated to the .org page");
  // wait for the browser to be loaded otherwise the task spawned in waitForRegistrationReady
  // might be destroyed (when it still belongs to the previous content process)
  await onBrowserLoaded;
  await waitForRegistrationReady(tab, ORG_PAGE_URL, ORG_WORKER_URL);

  const { hooks, commands, targetCommand } = await watchServiceWorkerTargets(
    tab
  );

  // We expect onAvailable to have been called one time, for the only service
  // worker target available in the test page.
  await checkHooks(hooks, {
    available: 1,
    destroyed: 0,
    targets: [ORG_WORKER_URL],
  });

  info("Unregister .org service worker and wait until onDestroyed is called.");
  await unregisterServiceWorker(ORG_WORKER_URL);
  await checkHooks(hooks, {
    available: 1,
    destroyed: 1,
    targets: [],
  });

  info("Go back .com page, wait for onAvailable to be called");
  onBrowserLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    COM_PAGE_URL
  );
  await onBrowserLoaded;

  await checkHooks(hooks, {
    available: 2,
    destroyed: 1,
    targets: [COM_WORKER_URL],
  });

  info("Unregister .com service worker and wait until onDestroyed is called.");
  await unregisterServiceWorker(COM_WORKER_URL);
  await checkHooks(hooks, {
    available: 2,
    destroyed: 2,
    targets: [],
  });

  // Stop listening to avoid worker related requests
  targetCommand.destroy();

  await commands.waitForRequestsToSettle();
  await commands.destroy();
  await removeTab(tab);
});

add_task(async function test_NavigationToPageWithExistingStoppedWorker() {
  await setupServiceWorkerNavigationTest();

  const tab = await addTab(COM_PAGE_URL);

  info("Wait until the service worker registration is registered");
  await waitForRegistrationReady(tab, COM_PAGE_URL, COM_WORKER_URL);

  await stopServiceWorker(COM_WORKER_URL);

  const { hooks, commands, targetCommand } = await watchServiceWorkerTargets(
    tab
  );

  // Let some time to watch target to eventually regress and revive the worker
  await wait(1000);

  // As the Service Worker doesn't have any active worker... it doesn't report any target.
  info(
    "Verify that no SW is reported after it has been stopped and we start watching for service workers"
  );
  await checkHooks(hooks, {
    available: 0,
    destroyed: 0,
    targets: [],
  });

  info("Reload the worker module via the postMessage call");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    const registration = await content.wrappedJSObject.registrationPromise;
    // Force loading the worker again, even it has been stopped
    registration.active.postMessage("");
  });

  info("Verify that the SW is notified");
  await checkHooks(hooks, {
    available: 1,
    destroyed: 0,
    targets: [COM_WORKER_URL],
  });

  await unregisterServiceWorker(COM_WORKER_URL);

  await checkHooks(hooks, {
    available: 1,
    destroyed: 1,
    targets: [],
  });

  // Stop listening to avoid worker related requests
  targetCommand.destroy();

  await commands.waitForRequestsToSettle();
  await commands.destroy();
  await removeTab(tab);
});

async function setupServiceWorkerNavigationTest() {
  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);
}

async function watchServiceWorkerTargets(tab) {
  info("Create a target list for a tab target");
  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;

  // Enable Service Worker listening.
  targetCommand.listenForServiceWorkers = true;
  await targetCommand.startListening();

  // Setup onAvailable & onDestroyed callbacks so that we can check how many
  // times they are called and with which targetFront.
  const hooks = {
    availableCount: 0,
    destroyedCount: 0,
    targets: [],
  };

  const onAvailable = async ({ targetFront }) => {
    info(` + Service worker target available for ${targetFront.url}\n`);
    hooks.availableCount++;
    hooks.targets.push(targetFront);
  };

  const onDestroyed = ({ targetFront }) => {
    info(` - Service worker target destroy for ${targetFront.url}\n`);
    hooks.destroyedCount++;
    hooks.targets.splice(hooks.targets.indexOf(targetFront), 1);
  };

  await targetCommand.watchTargets({
    types: [targetCommand.TYPES.SERVICE_WORKER],
    onAvailable,
    onDestroyed,
  });

  return { hooks, commands, targetCommand };
}

/**
 * Wait until the expected URL is loaded and win.registration has resolved.
 */
async function waitForRegistrationReady(tab, expectedPageUrl, workerUrl) {
  await asyncWaitUntil(() =>
    SpecialPowers.spawn(tab.linkedBrowser, [expectedPageUrl], function (_url) {
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
  // On debug builds, the registration may not be yet ready in the parent process
  // so we also need to ensure it is ready.
  const swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
    Ci.nsIServiceWorkerManager
  );
  await waitFor(() => {
    // Unfortunately we can't use swm.getRegistrationByPrincipal, as it requires a "scope", which doesn't seem to be the worker URL.
    const registrations = swm.getAllRegistrations();
    for (let i = 0; i < registrations.length; i++) {
      const info = registrations.queryElementAt(
        i,
        Ci.nsIServiceWorkerRegistrationInfo
      );
      // Lookup for an exact URL match.
      if (info.scriptSpec === workerUrl) {
        return true;
      }
    }
    return false;
  });
}

/**
 * Assert helper for the `hooks` object, updated by the onAvailable and
 * onDestroyed callbacks. Assert that the callbacks have been called the
 * expected number of times, with the expected targets.
 */
async function checkHooks(hooks, { available, destroyed, targets }) {
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
