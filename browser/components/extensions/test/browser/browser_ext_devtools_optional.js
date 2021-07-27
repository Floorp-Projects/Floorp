/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

loadTestSubscript("head_devtools.js");

/**
 * This test file ensures that:
 *
 * - "devtools" permission can be used as an optional permission
 * - the extension devtools page and panels are not disabled/enabled on changes
 *   to unrelated optional permissions.
 */
add_task(async function test_devtools_optional_permission() {
  Services.prefs.setBoolPref(
    "extensions.webextOptionalPermissionPrompts",
    false
  );
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("extensions.webextOptionalPermissionPrompts");
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888/"
  );

  function background() {
    browser.test.onMessage.addListener(async (msg, perm) => {
      if (msg === "request") {
        let granted = await new Promise(resolve => {
          browser.test.withHandlingUserInput(() => {
            resolve(browser.permissions.request(perm));
          });
        });
        browser.test.assertTrue(granted, "permission request succeeded");
        browser.test.sendMessage("done");
      } else if (msg === "revoke") {
        await browser.permissions.remove(perm);
        browser.test.sendMessage("done");
      }
    });
  }

  function devtools_page() {
    browser.test.sendMessage("devtools_page_loaded");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      optional_permissions: ["devtools", "*://mochi.test/*"],
      devtools_page: "devtools_page.html",
    },
    files: {
      "devtools_page.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
          </head>
          <body>
            <script src="devtools_page.js"></script>
          </body>
        </html>`,
      "devtools_page.js": devtools_page,
    },
  });

  await extension.startup();

  function checkEnabled(expect = false, { expectIsUserSet = true } = {}) {
    const prefName = `devtools.webextensions.${extension.id}.enabled`;
    Assert.equal(
      expect,
      Services.prefs.getBoolPref(prefName, false),
      `Got the expected value set on pref ${prefName}`
    );

    Assert.equal(
      expectIsUserSet,
      Services.prefs.prefHasUserValue(prefName),
      `pref "${prefName}" ${
        expectIsUserSet ? "should" : "should not"
      } be user set`
    );
  }

  checkEnabled(false, { expectIsUserSet: false });

  // Open the devtools first, then request permission
  info("Open the developer toolbox");
  await openToolboxForTab(tab);
  assertDevToolsExtensionEnabled(extension.uuid, false);

  info(
    "Request unrelated permission, expect devtools page and panel to be disabled"
  );
  extension.sendMessage("request", {
    permissions: [],
    origins: ["*://mochi.test/*"],
  });
  await extension.awaitMessage("done");
  checkEnabled(false, { expectIsUserSet: false });

  info(
    "Request devtools permission, expect devtools page and panel to be enabled"
  );
  extension.sendMessage("request", {
    permissions: ["devtools"],
    origins: [],
  });
  await extension.awaitMessage("done");
  checkEnabled(true);

  info("Wait the devtools page load");
  await extension.awaitMessage("devtools_page_loaded");
  assertDevToolsExtensionEnabled(extension.uuid, true);

  info(
    "Revoke unrelated permission, expect devtools page and panel to stay enabled"
  );
  extension.sendMessage("revoke", {
    permissions: [],
    origins: ["*://mochi.test/*"],
  });
  await extension.awaitMessage("done");
  checkEnabled(true);

  info(
    "Revoke devtools permission, expect devtools page and panel to be destroyed"
  );
  let policy = WebExtensionPolicy.getByID(extension.id);
  let closed = new Promise(resolve => {
    // eslint-disable-next-line mozilla/balanced-listeners
    policy.extension.on("devtools-page-shutdown", resolve);
  });

  extension.sendMessage("revoke", {
    permissions: ["devtools"],
    origins: [],
  });
  await extension.awaitMessage("done");

  await closed;
  checkEnabled(false);
  assertDevToolsExtensionEnabled(extension.uuid, false);

  info("Close the developer toolbox");
  await closeToolboxForTab(tab);

  extension.sendMessage("request", {
    permissions: ["devtools"],
    origins: [],
  });
  await extension.awaitMessage("done");

  info("Open the developer toolbox");
  openToolboxForTab(tab);

  checkEnabled(true);

  info("Wait the devtools page load");
  await extension.awaitMessage("devtools_page_loaded");
  assertDevToolsExtensionEnabled(extension.uuid, true);
  await extension.unload();

  await closeToolboxForTab(tab);
  BrowserTestUtils.removeTab(tab);
});
