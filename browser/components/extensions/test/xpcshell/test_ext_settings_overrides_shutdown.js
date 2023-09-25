/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
// Lazily import ExtensionParent to allow AddonTestUtils.createAppInfo to
// override Services.appinfo.
ChromeUtils.defineESModuleGetters(this, {
  ExtensionParent: "resource://gre/modules/ExtensionParent.sys.mjs",
});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

add_task(async function shutdown_during_search_provider_startup() {
  await AddonTestUtils.promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          is_default: true,
          name: "dummy name",
          search_url: "https://example.com/",
        },
      },
    },
  });

  info("Starting up search extension");
  await extension.startup();
  let extStartPromise = AddonTestUtils.waitForSearchProviderStartup(extension, {
    // Search provider registration is expected to be pending because the search
    // service has not been initialized yet.
    expectPending: true,
  });

  let initialized = false;
  Services.search.promiseInitialized.then(() => {
    initialized = true;
  });

  await extension.addon.disable();

  info("Extension managed to shut down despite the uninitialized search");
  // Initialize search after extension shutdown to check that it does not cause
  // any problems, and that the test can continue to test uninstall behavior.
  Assert.ok(!initialized, "Search service should not have been initialized");

  extension.addon.enable();
  await extension.awaitStartup();

  // Check that uninstall is blocked until the search registration at startup
  // has finished. This registration only finished once the search service is
  // initialized.
  let uninstallingPromise = new Promise(resolve => {
    let Management = ExtensionParent.apiManager;
    Management.on("uninstall", function listener(eventName, { id }) {
      Management.off("uninstall", listener);
      Assert.equal(id, extension.id, "Expected extension");
      resolve();
    });
  });

  let extRestartPromise = AddonTestUtils.waitForSearchProviderStartup(
    extension,
    {
      // Search provider registration is expected to be pending again,
      // because the search service has still not been initialized yet.
      expectPending: true,
    }
  );

  let uninstalledPromise = extension.addon.uninstall();
  let uninstalled = false;
  uninstalledPromise.then(() => {
    uninstalled = true;
  });

  await uninstallingPromise;
  Assert.ok(!uninstalled, "Uninstall should not be finished yet");
  Assert.ok(!initialized, "Search service should still be uninitialized");
  await Services.search.init();
  Assert.ok(initialized, "Search service should be initialized");

  // After initializing the search service, the search provider registration
  // promises should settle eventually.

  // Despite the interrupted startup, the promise should still resolve without
  // an error.
  await extStartPromise;
  // The extension that is still active. The promise should just resolve.
  await extRestartPromise;

  // After initializing the search service, uninstall should eventually finish.
  await uninstalledPromise;

  await AddonTestUtils.promiseShutdownManager();
});
