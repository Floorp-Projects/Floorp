/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

loadTestSubscript("head_devtools.js");

/**
 * This test file ensures that:
 *
 * - the devtools_page property creates a new WebExtensions context
 * - the devtools_page can exchange messages with the background page
 */
add_task(async function test_devtools_page_runtime_api_messaging() {
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
    let perm = { permissions: ["devtools"], origins: [] };
    browser.test.onMessage.addListener(async (msg, sender) => {
      if (msg === "request") {
        let granted = await new Promise(resolve => {
          browser.test.withHandlingUserInput(() => {
            resolve(browser.permissions.request(perm));
          });
        });
        browser.test.assertTrue(granted, "permission request succeeded");
        browser.test.sendMessage("done");
      } else if (msg === "revoke") {
        browser.permissions.remove(perm);
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
      optional_permissions: ["devtools"],
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

  function checkEnabled(expect = false) {
    Assert.equal(
      expect,
      Services.prefs.getBoolPref(
        `devtools.webextensions.${extension.id}.enabled`,
        false
      ),
      "devtools enabled pref is correct"
    );
  }

  checkEnabled(false);

  // Open the devtools first, then request permission
  info("Open the developer toolbox");
  await openToolboxForTab(tab);
  assertDevToolsExtensionEnabled(extension.uuid, false);

  extension.sendMessage("request");
  await extension.awaitMessage("done");
  checkEnabled(true);

  info("Wait the devtools page load");
  await extension.awaitMessage("devtools_page_loaded");
  assertDevToolsExtensionEnabled(extension.uuid, true);

  let policy = WebExtensionPolicy.getByID(extension.id);
  let closed = new Promise(resolve => {
    // eslint-disable-next-line mozilla/balanced-listeners
    policy.extension.on("devtools-page-shutdown", resolve);
  });

  extension.sendMessage("revoke");
  await extension.awaitMessage("done");

  await closed;
  checkEnabled(false);
  assertDevToolsExtensionEnabled(extension.uuid, false);

  await extension.unload();

  await closeToolboxForTab(tab);
  BrowserTestUtils.removeTab(tab);
});
