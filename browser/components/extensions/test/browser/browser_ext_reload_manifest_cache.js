"use strict";

const { ExtensionPermissions } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
);

add_task(async function test_reload_manifest_startupcache() {
  const id = "id@tests.mozilla.org";

  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: { id },
      },
      options_ui: {
        open_in_tab: true,
        page: "options.html",
      },
      optional_permissions: ["<all_urls>"],
    },
    useAddonManager: "temporary",
    files: {
      "options.html": `lol`,
    },
    background() {
      browser.runtime.openOptionsPage();
      browser.permissions.onAdded.addListener(() => {
        browser.runtime.openOptionsPage();
      });
    },
  });

  async function waitOptionsTab() {
    let tab = await BrowserTestUtils.waitForNewTab(gBrowser, url =>
      url.endsWith("options.html")
    );
    BrowserTestUtils.removeTab(tab);
  }

  // Open a non-blank tab to force options to open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );
  let optionsTabPromise = waitOptionsTab();

  await ext.startup();
  await optionsTabPromise;

  let disabledPromise = awaitEvent("shutdown", id);
  let enabledPromise = awaitEvent("ready", id);
  optionsTabPromise = waitOptionsTab();

  let addon = await AddonManager.getAddonByID(id);
  await addon.reload();

  await Promise.all([disabledPromise, enabledPromise, optionsTabPromise]);

  optionsTabPromise = waitOptionsTab();
  ExtensionPermissions.add(id, {
    permissions: [],
    origins: ["<all_urls>"],
  });
  await optionsTabPromise;

  let policy = WebExtensionPolicy.getByID(id);
  let optionsUrl = policy.extension.manifest.options_ui.page;
  ok(optionsUrl.includes(policy.mozExtensionHostname), "Normalized manifest.");

  await BrowserTestUtils.removeTab(tab);
  await ext.unload();
});
