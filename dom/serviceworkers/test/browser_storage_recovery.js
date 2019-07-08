"use strict";

// This test registers a SW for a scope that will never control a document
// and therefore never trigger a "fetch" functional event that would
// automatically attempt to update the registration.  The overlap of the
// PAGE_URI and SCOPE is incidental.  checkForUpdate is the only thing that
// will trigger an update of the registration and so there is no need to
// worry about Schedule Job races to coalesce an update job.

const BASE_URI = "http://mochi.test:8888/browser/dom/serviceworkers/test/";
const PAGE_URI = BASE_URI + "empty.html";
const SCOPE = PAGE_URI + "?storage_recovery";
const SW_SCRIPT = BASE_URI + "storage_recovery_worker.sjs";

async function checkForUpdate(browser) {
  return ContentTask.spawn(browser, SCOPE, async function(uri) {
    let reg = await content.navigator.serviceWorker.getRegistration(uri);
    await reg.update();
    return !!reg.installing;
  });
}

// Delete all of our chrome-namespace Caches for this origin, leaving any
// content-owned caches in place.  This is exclusively for simulating loss
// of the origin's storage without loss of the registration and without
// having to worry that future enhancements to QuotaClients/ServiceWorkerRegistrar
// will break this test.  If you want to wipe storage for an origin, use
// QuotaManager APIs
async function wipeStorage(u) {
  let uri = Services.io.newURI(u);
  let principal = Services.scriptSecurityManager.createCodebasePrincipal(
    uri,
    {}
  );
  let caches = new CacheStorage("chrome", principal);
  let list = await caches.keys();
  return Promise.all(list.map(c => caches.delete(c)));
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Until the e10s refactor is complete, use a single process to avoid
      // service worker propagation race.
      ["dom.ipc.processCount", 1],
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.testing.enabled", true],
      ["dom.serviceWorkers.idle_timeout", 0],
    ],
  });

  // Configure the server script to not redirect.
  await fetch(SW_SCRIPT + "?clear-redirect");

  let tab = BrowserTestUtils.addTab(gBrowser, PAGE_URI);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await ContentTask.spawn(
    browser,
    { script: SW_SCRIPT, scope: SCOPE },
    async function(opts) {
      let reg = await content.navigator.serviceWorker.register(opts.script, {
        scope: opts.scope,
      });
      let worker = reg.installing || reg.waiting || reg.active;
      await new Promise(resolve => {
        if (worker.state === "activated") {
          resolve();
          return;
        }
        worker.addEventListener("statechange", function onStateChange() {
          if (worker.state === "activated") {
            worker.removeEventListener("statechange", onStateChange);
            resolve();
          }
        });
      });
    }
  );

  BrowserTestUtils.removeTab(tab);
});

// Verify that our service worker doesn't update normally.
add_task(async function normal_update_check() {
  let tab = BrowserTestUtils.addTab(gBrowser, PAGE_URI);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let updated = await checkForUpdate(browser);
  ok(!updated, "normal update check should not trigger an update");

  BrowserTestUtils.removeTab(tab);
});

// Test what happens when we wipe the service worker scripts
// out from under the site before triggering the update.  This
// should cause an update to occur.
add_task(async function wiped_update_check() {
  // Wipe the backing cache storage, but leave the SW registered.
  await wipeStorage(PAGE_URI);

  let tab = BrowserTestUtils.addTab(gBrowser, PAGE_URI);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let updated = await checkForUpdate(browser);
  ok(updated, "wiping the service worker scripts should trigger an update");

  BrowserTestUtils.removeTab(tab);
});

// Test what happens when we wipe the service worker scripts
// out from under the site before triggering the update.  This
// should cause an update to occur.
add_task(async function wiped_and_failed_update_check() {
  // Wipe the backing cache storage, but leave the SW registered.
  await wipeStorage(PAGE_URI);

  // Configure the service worker script to redirect.  This will
  // prevent the update from completing successfully.
  await fetch(SW_SCRIPT + "?set-redirect");

  let tab = BrowserTestUtils.addTab(gBrowser, PAGE_URI);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  // Attempt to update the service worker.  This should throw
  // an error because the script is now redirecting.
  let updateFailed = false;
  try {
    await checkForUpdate(browser);
  } catch (e) {
    updateFailed = true;
  }
  ok(updateFailed, "redirecting service worker script should fail to update");

  // Also, since the existing service worker's scripts are broken
  // we should also remove the registration completely when the
  // update fails.
  let exists = await ContentTask.spawn(browser, SCOPE, async function(uri) {
    let reg = await content.navigator.serviceWorker.getRegistration(uri);
    return !!reg;
  });
  ok(
    !exists,
    "registration should be removed after scripts are wiped and update fails"
  );

  // Note, we don't have to clean up the service worker registration
  // since its effectively been force-removed here.

  BrowserTestUtils.removeTab(tab);
});
