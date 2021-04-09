/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Load lazy so we create the app info first.
ChromeUtils.defineModuleGetter(
  this,
  "PageActions",
  "resource:///modules/PageActions.jsm"
);

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "58");

// This is copied and pasted from ExtensionPopups.jsm.  It's used as the
// PageActions action ID.  See ext-pageAction.js.
function makeWidgetId(id) {
  id = id.toLowerCase();
  // FIXME: This allows for collisions.
  return id.replace(/[^a-z0-9_-]/g, "_");
}

// Tests that the pinnedToUrlbar property of the PageActions.Action object
// backing the extension's page action persists across app restarts.
add_task(async function testAppShutdown() {
  let extensionData = {
    useAddonManager: "permanent",
    manifest: {
      page_action: {
        default_title: "test_ext_pageAction_shutdown.js",
        browser_style: false,
      },
    },
  };

  // Simulate starting up the app.
  PageActions.init();
  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  // Get the PageAction.Action object.  Its pinnedToUrlbar should have been
  // initialized to true in ext-pageAction.js, when it's created.
  let actionID = makeWidgetId(extension.id);
  let action = PageActions.actionForID(actionID);
  Assert.equal(action.pinnedToUrlbar, true);

  // Simulate restarting the app without first unloading the extension.
  await promiseShutdownManager();
  PageActions._reset();
  await promiseStartupManager();
  await extension.awaitStartup();

  // Get the action.  Its pinnedToUrlbar should remain true.
  action = PageActions.actionForID(actionID);
  Assert.equal(action.pinnedToUrlbar, true);

  // Now set its pinnedToUrlbar to false.
  action.pinnedToUrlbar = false;

  // Simulate restarting the app again without first unloading the extension.
  await promiseShutdownManager();
  PageActions._reset();
  await promiseStartupManager();
  await extension.awaitStartup();

  // Get the action.  In non-proton its pinnedToUrlbar should remain false.
  // In Proton there is no meatball menu, thus page actions are directly pinned
  // to the urlbar.
  action = PageActions.actionForID(actionID);
  Assert.equal(
    action.pinnedToUrlbar,
    Services.prefs.getBoolPref("browser.proton.enabled", false)
  );

  // Now unload the extension and quit the app.
  await extension.unload();
  await promiseShutdownManager();
});
