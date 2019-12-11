/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionSettingsStore",
  "resource://gre/modules/ExtensionSettingsStore.jsm"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "aboutNewTabService",
  "@mozilla.org/browser/aboutnewtab-service;1",
  "nsIAboutNewTabService"
);

const NEWTAB_URI_1 = "webext-newtab-1.html";

function getNotificationSetting(extensionId) {
  return ExtensionSettingsStore.getSetting("newTabNotification", extensionId);
}

function getNewTabDoorhanger() {
  return document.getElementById("extension-new-tab-notification");
}

function clickKeepChanges(notification) {
  notification.button.click();
}

function clickRestoreSettings(notification) {
  notification.secondaryButton.click();
}

function waitForNewTab() {
  let eventName = "browser-open-newtab-start";
  return new Promise(resolve => {
    function observer() {
      Services.obs.removeObserver(observer, eventName);
      resolve();
    }
    Services.obs.addObserver(observer, eventName);
  });
}

function waitForAddonDisabled(addon) {
  return new Promise(resolve => {
    let listener = {
      onDisabled(disabledAddon) {
        if (disabledAddon.id == addon.id) {
          resolve();
          AddonManager.removeAddonListener(listener);
        }
      },
    };
    AddonManager.addAddonListener(listener);
  });
}

function waitForAddonEnabled(addon) {
  return new Promise(resolve => {
    let listener = {
      onEnabled(enabledAddon) {
        if (enabledAddon.id == addon.id) {
          AddonManager.removeAddonListener(listener);
          resolve();
        }
      },
    };
    AddonManager.addAddonListener(listener);
  });
}

add_task(async function test_new_tab_opens() {
  let panel = getNewTabDoorhanger().closest("panel");
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_url_overrides: {
        newtab: NEWTAB_URI_1,
      },
    },
    useAddonManager: "temporary",
    files: {
      [NEWTAB_URI_1]: `
        <!DOCTYPE html>
        <head>
          <meta charset="utf-8"/></head>
        <html>
          <body>
            <script src="newtab.js"></script>
          </body>
        </html>
      `,

      "newtab.js": function() {
        window.onload = () => {
          browser.test.sendMessage("from-newtab-page", window.location.href);
        };
      },
    },
  });

  await extension.startup();

  // Simulate opening the newtab open as a user would.
  let popupShown = promisePopupShown(panel);
  BrowserOpenTab();
  await popupShown;

  let url = await extension.awaitMessage("from-newtab-page");
  ok(url.endsWith(NEWTAB_URI_1), "Newtab url is overridden by the extension.");

  // This will show a confirmation doorhanger, make sure we don't leave it open.
  let popupHidden = promisePopupHidden(panel);
  panel.hidePopup();
  await popupHidden;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await extension.unload();
});

add_task(async function test_new_tab_ignore_settings() {
  await ExtensionSettingsStore.initialize();
  let notification = getNewTabDoorhanger();
  let panel = notification.closest("panel");
  let extensionId = "newtabignore@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: extensionId } },
      browser_action: { default_popup: "ignore.html" },
      chrome_url_overrides: { newtab: "ignore.html" },
    },
    files: { "ignore.html": '<h1 id="extension-new-tab">New Tab!</h1>' },
    useAddonManager: "temporary",
  });

  ok(
    panel.getAttribute("panelopen") != "true",
    "The notification panel is initially closed"
  );

  await extension.startup();

  // Simulate opening the New Tab as a user would.
  let popupShown = promisePopupShown(panel);
  BrowserOpenTab();
  await popupShown;

  // Ensure the doorhanger is shown and the setting isn't set yet.
  is(
    panel.getAttribute("panelopen"),
    "true",
    "The notification panel is open after opening New Tab"
  );
  is(gURLBar.focused, false, "The URL bar is not focused with a doorhanger");
  is(
    getNotificationSetting(extensionId),
    null,
    "The New Tab notification is not set for this extension"
  );
  is(
    panel.anchorNode.closest("toolbarbutton").id,
    "newtabignore_mochi_test-browser-action",
    "The doorhanger is anchored to the browser action"
  );

  // Manually close the panel, as if the user ignored it.
  let popupHidden = promisePopupHidden(panel);
  panel.hidePopup();
  await popupHidden;

  // Ensure panel is closed and the setting still isn't set.
  ok(
    panel.getAttribute("panelopen") != "true",
    "The notification panel is closed"
  );
  is(
    getNotificationSetting(extensionId),
    null,
    "The New Tab notification is not set after ignoring the doorhanger"
  );

  // Close the first tab and open another new tab.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  let newTabOpened = waitForNewTab();
  BrowserOpenTab();
  await newTabOpened;

  // Verify the doorhanger is not shown a second time.
  ok(
    panel.getAttribute("panelopen") != "true",
    "The notification panel doesn't open after ignoring the doorhanger"
  );
  is(gURLBar.focused, true, "The URL bar is focused with no doorhanger");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await extension.unload();
});

add_task(async function test_new_tab_keep_settings() {
  await ExtensionSettingsStore.initialize();
  let notification = getNewTabDoorhanger();
  let panel = notification.closest("panel");
  let extensionId = "newtabkeep@mochi.test";
  let manifest = {
    version: "1.0",
    name: "New Tab Add-on",
    applications: { gecko: { id: extensionId } },
    chrome_url_overrides: { newtab: "keep.html" },
  };
  let files = {
    "keep.html":
      '<script src="newtab.js"></script><h1 id="extension-new-tab">New Tab!</h1>',
    "newtab.js": () => {
      window.onload = browser.test.sendMessage("newtab");
    },
  };
  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    files,
    useAddonManager: "permanent",
  });

  ok(
    panel.getAttribute("panelopen") != "true",
    "The notification panel is initially closed"
  );

  await extension.startup();

  // Simulate opening the New Tab as a user would.
  let popupShown = promisePopupShown(panel);
  BrowserOpenTab();
  await extension.awaitMessage("newtab");
  await popupShown;

  // Ensure the panel is open and the setting isn't saved yet.
  is(
    panel.getAttribute("panelopen"),
    "true",
    "The notification panel is open after opening New Tab"
  );
  is(
    getNotificationSetting(extensionId),
    null,
    "The New Tab notification is not set for this extension"
  );
  is(
    panel.anchorNode.closest("toolbarbutton").id,
    "PanelUI-menu-button",
    "The doorhanger is anchored to the menu icon"
  );
  is(
    panel.querySelector("#extension-new-tab-notification-description")
      .textContent,
    "An extension,  New Tab Add-on, changed the page you see when you open a new tab.Learn more",
    "The description includes the add-on name"
  );

  // Click the Keep Changes button.
  let confirmationSaved = TestUtils.waitForCondition(() => {
    return ExtensionSettingsStore.getSetting(
      "newTabNotification",
      extensionId,
      extensionId
    ).value;
  });
  clickKeepChanges(notification);
  await confirmationSaved;

  // Ensure panel is closed and setting is updated.
  ok(
    panel.getAttribute("panelopen") != "true",
    "The notification panel is closed after click"
  );
  is(
    getNotificationSetting(extensionId).value,
    true,
    "The New Tab notification is set after keeping the changes"
  );

  // Close the first tab and open another new tab.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  BrowserOpenTab();
  await extension.awaitMessage("newtab");

  // Verify the doorhanger is not shown a second time.
  ok(
    panel.getAttribute("panelopen") != "true",
    "The notification panel is not opened after keeping the changes"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  let upgradedExtension = ExtensionTestUtils.loadExtension({
    manifest: Object.assign({}, manifest, { version: "2.0" }),
    files,
    useAddonManager: "permanent",
  });

  await upgradedExtension.startup();

  BrowserOpenTab();
  await upgradedExtension.awaitMessage("newtab");

  // Ensure panel is closed and setting is still set.
  ok(
    panel.getAttribute("panelopen") != "true",
    "The notification panel is closed after click"
  );
  is(
    getNotificationSetting(extensionId).value,
    true,
    "The New Tab notification is set after keeping the changes"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await upgradedExtension.unload();
  await extension.unload();

  let confirmation = ExtensionSettingsStore.getSetting(
    "newTabNotification",
    extensionId,
    extensionId
  );
  is(confirmation, null, "The confirmation has been cleaned up");
});

add_task(async function test_new_tab_restore_settings() {
  await ExtensionSettingsStore.initialize();
  let notification = getNewTabDoorhanger();
  let panel = notification.closest("panel");
  let extensionId = "newtabrestore@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: extensionId } },
      chrome_url_overrides: { newtab: "restore.html" },
    },
    files: { "restore.html": '<h1 id="extension-new-tab">New Tab!</h1>' },
    useAddonManager: "temporary",
  });

  ok(
    panel.getAttribute("panelopen") != "true",
    "The notification panel is initially closed"
  );
  is(
    getNotificationSetting(extensionId),
    null,
    "The New Tab notification is not initially set for this extension"
  );

  await extension.startup();

  // Simulate opening the newtab open as a user would.
  let popupShown = promisePopupShown(panel);
  BrowserOpenTab();
  await popupShown;

  // Verify that the panel is open and add-on is enabled.
  let addon = await AddonManager.getAddonByID(extensionId);
  is(addon.userDisabled, false, "The add-on is enabled at first");
  is(
    panel.getAttribute("panelopen"),
    "true",
    "The notification panel is open after opening New Tab"
  );
  is(
    getNotificationSetting(extensionId),
    null,
    "The New Tab notification is not set for this extension"
  );

  // Click the Restore Changes button.
  let addonDisabled = waitForAddonDisabled(addon);
  let popupHidden = promisePopupHidden(panel);
  let locationChanged = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    "about:newtab"
  );
  clickRestoreSettings(notification);
  await popupHidden;
  await addonDisabled;
  await locationChanged;

  // Ensure panel is closed, settings haven't changed and add-on is disabled.
  ok(
    panel.getAttribute("panelopen") != "true",
    "The notification panel is closed after click"
  );
  is(
    getNotificationSetting(extensionId),
    null,
    "The New Tab notification is not set after restoring the settings"
  );
  is(addon.userDisabled, true, "The extension is now disabled");
  is(
    gBrowser.currentURI.spec,
    "about:newtab",
    "The user has been redirected to about:newtab"
  );

  // Wait for the next event tick to make sure the remaining part of the test
  // is not executed inside tabbrowser's onLocationChange.
  // See bug 1416153 for more details.
  await TestUtils.waitForTick();

  // Reopen a browser tab and verify that there's no doorhanger.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  let newTabOpened = waitForNewTab();
  BrowserOpenTab();
  await newTabOpened;

  ok(
    panel.getAttribute("panelopen") != "true",
    "The notification panel is not opened after keeping the changes"
  );

  // FIXME: We need to enable the add-on so it gets cleared from the
  // ExtensionSettingsStore for now. See bug 1408226.
  let addonEnabled = new Promise(resolve => {
    let listener = {
      onEnabled(enabledAddon) {
        if (enabledAddon.id == addon.id) {
          AddonManager.removeAddonListener(listener);
          resolve();
        }
      },
    };
    AddonManager.addAddonListener(listener);
  });
  await addon.enable();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await addonEnabled;
  await extension.unload();
});

add_task(async function test_new_tab_restore_settings_multiple() {
  await ExtensionSettingsStore.initialize();
  let notification = getNewTabDoorhanger();
  let panel = notification.closest("panel");
  let extensionOneId = "newtabrestoreone@mochi.test";
  let extensionOne = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: extensionOneId } },
      chrome_url_overrides: { newtab: "restore-one.html" },
    },
    files: {
      "restore-one.html": `
        <h1 id="extension-new-tab">New Tab!</h1>
        <script src="newtab.js"></script>
      `,
      "newtab.js": function() {
        browser.test.sendMessage("from-newtab-page", window.location.href);
      },
    },
    useAddonManager: "temporary",
  });
  let extensionTwoId = "newtabrestoretwo@mochi.test";
  let extensionTwo = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: extensionTwoId } },
      chrome_url_overrides: { newtab: "restore-two.html" },
    },
    files: { "restore-two.html": '<h1 id="extension-new-tab">New Tab!</h1>' },
    useAddonManager: "temporary",
  });

  ok(
    panel.getAttribute("panelopen") != "true",
    "The notification panel is initially closed"
  );
  is(
    getNotificationSetting(extensionOneId),
    null,
    "The New Tab notification is not initially set for this extension"
  );
  is(
    getNotificationSetting(extensionTwoId),
    null,
    "The New Tab notification is not initially set for this extension"
  );

  await extensionOne.startup();
  await extensionTwo.startup();

  // Simulate opening the newtab open as a user would.
  let popupShown = promisePopupShown(panel);
  BrowserOpenTab();
  await popupShown;

  // Verify that the panel is open and add-on is enabled.
  let addonTwo = await AddonManager.getAddonByID(extensionTwoId);
  is(addonTwo.userDisabled, false, "The add-on is enabled at first");
  is(
    panel.getAttribute("panelopen"),
    "true",
    "The notification panel is open after opening New Tab"
  );
  is(
    getNotificationSetting(extensionTwoId),
    null,
    "The New Tab notification is not set for this extension"
  );

  // Click the Restore Changes button.
  let addonDisabled = waitForAddonDisabled(addonTwo);
  let popupHidden = promisePopupHidden(panel);
  let newTabUrlPromise = extensionOne.awaitMessage("from-newtab-page");
  clickRestoreSettings(notification);
  await popupHidden;
  await addonDisabled;
  await promisePopupShown(panel);
  let newTabUrl = await newTabUrlPromise;

  // Ensure the panel opens again for the next add-on.
  is(
    getNotificationSetting(extensionTwoId),
    null,
    "The New Tab notification is not set after restoring the settings"
  );
  is(addonTwo.userDisabled, true, "The extension is now disabled");
  let addonOne = await AddonManager.getAddonByID(extensionOneId);
  is(
    addonOne.userDisabled,
    false,
    "The extension is enabled before making a choice"
  );
  is(
    getNotificationSetting(extensionOneId),
    null,
    "The New Tab notification is not set before making a choice"
  );
  is(
    panel.getAttribute("panelopen"),
    "true",
    "The notification panel is open after opening New Tab"
  );
  is(
    gBrowser.currentURI.spec,
    newTabUrl,
    "The user is now on the next extension's New Tab page"
  );

  addonDisabled = waitForAddonDisabled(addonOne);
  popupHidden = promisePopupHidden(panel);
  let locationChanged = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    "about:newtab"
  );
  clickRestoreSettings(notification);
  await popupHidden;
  await addonDisabled;
  await locationChanged;

  ok(
    panel.getAttribute("panelopen") != "true",
    "The notification panel is closed after restoring the second time"
  );
  is(
    getNotificationSetting(extensionOneId),
    null,
    "The New Tab notification is not set after restoring the settings"
  );
  is(addonOne.userDisabled, true, "The extension is now disabled");
  is(
    gBrowser.currentURI.spec,
    "about:newtab",
    "The user is now on the original New Tab URL since all extensions are disabled"
  );

  // Wait for the next event tick to make sure the remaining part of the test
  // is not executed inside tabbrowser's onLocationChange.
  // See bug 1416153 for more details.
  await TestUtils.waitForTick();

  // Reopen a browser tab and verify that there's no doorhanger.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  let newTabOpened = waitForNewTab();
  BrowserOpenTab();
  await newTabOpened;

  ok(
    panel.getAttribute("panelopen") != "true",
    "The notification panel is not opened after keeping the changes"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // FIXME: We need to enable the add-on so it gets cleared from the
  // ExtensionSettingsStore for now. See bug 1408226.
  let addonsEnabled = Promise.all([
    waitForAddonEnabled(addonOne),
    waitForAddonEnabled(addonTwo),
  ]);
  await addonOne.enable();
  await addonTwo.enable();
  await addonsEnabled;
  await extensionOne.unload();
  await extensionTwo.unload();
});

/**
 * Ensure we don't show the extension URL in the URL bar temporarily in new tabs
 * while we're switching remoteness (when the URL we're loading and the
 * default content principal are different).
 */
add_task(async function dontTemporarilyShowAboutExtensionPath() {
  await ExtensionSettingsStore.initialize();
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test Extension",
      applications: {
        gecko: {
          id: "newtaburl@mochi.test",
        },
      },
      chrome_url_overrides: {
        newtab: "newtab.html",
      },
    },
    background() {
      browser.test.sendMessage("url", browser.runtime.getURL("newtab.html"));
    },
    files: {
      "newtab.html": "<h1>New tab!</h1>",
    },
    useAddonManager: "temporary",
  });

  await extension.startup();
  let url = await extension.awaitMessage("url");

  let wpl = {
    onLocationChange() {
      is(gURLBar.value, "", "URL bar value should stay empty.");
    },
  };
  gBrowser.addProgressListener(wpl);

  let tab = await BrowserTestUtils.openNewForegroundTab({ gBrowser, url });

  gBrowser.removeProgressListener(wpl);
  is(gURLBar.value, "", "URL bar value should be empty.");
  ContentTask.spawn(tab.linkedBrowser, null, function() {
    is(
      content.document.body.textContent,
      "New tab!",
      "New tab page is loaded."
    );
  });

  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});

add_task(async function test_overriding_newtab_incognito_not_allowed() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.allowPrivateBrowsingByDefault", false]],
  });

  let panel = getNewTabDoorhanger().closest("panel");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_url_overrides: { newtab: "newtab.html" },
      name: "extension",
      applications: {
        gecko: { id: "@not-allowed-newtab" },
      },
    },
    files: {
      "newtab.html": `
        <!DOCTYPE html>
        <head>
          <meta charset="utf-8"/></head>
        <html>
          <body>
            <script src="newtab.js"></script>
          </body>
        </html>
      `,

      "newtab.js": function() {
        window.onload = () => {
          browser.test.sendMessage("from-newtab-page", window.location.href);
        };
      },
    },
    useAddonManager: "permanent",
  });

  await extension.startup();

  let popupShown = promisePopupShown(panel);
  BrowserOpenTab();
  await popupShown;

  let url = await extension.awaitMessage("from-newtab-page");
  ok(url.endsWith("newtab.html"), "Newtab url is overridden by the extension.");

  // This will show a confirmation doorhanger, make sure we don't leave it open.
  let popupHidden = promisePopupHidden(panel);
  panel.hidePopup();
  await popupHidden;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // Verify a private window does not open the extension page.  We would
  // get an extra notification that we don't listen for if it gets loaded.
  let windowOpenedPromise = BrowserTestUtils.waitForNewWindow();
  let win = OpenBrowserWindow({ private: true });
  await windowOpenedPromise;

  let newTabOpened = waitForNewTab();
  win.BrowserOpenTab();
  await newTabOpened;

  is(win.gURLBar.value, "", "newtab not used in private window");

  // Verify setting the pref directly doesn't bypass permissions.
  let origUrl = aboutNewTabService.newTabURL;
  aboutNewTabService.newTabURL = url;
  newTabOpened = waitForNewTab();
  win.BrowserOpenTab();
  await newTabOpened;

  is(win.gURLBar.value, "", "directly set newtab not used in private window");

  aboutNewTabService.newTabURL = origUrl;

  await extension.unload();
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_overriding_newtab_incognito_spanning() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.allowPrivateBrowsingByDefault", false]],
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_url_overrides: { newtab: "newtab.html" },
      name: "extension",
      applications: {
        gecko: { id: "@spanning-newtab" },
      },
    },
    files: {
      "newtab.html": `
        <!DOCTYPE html>
        <head>
          <meta charset="utf-8"/></head>
        <html>
          <body>
            <script src="newtab.js"></script>
          </body>
        </html>
      `,

      "newtab.js": function() {
        window.onload = () => {
          browser.test.sendMessage("from-newtab-page", window.location.href);
        };
      },
    },
    useAddonManager: "permanent",
    incognitoOverride: "spanning",
  });

  await extension.startup();

  let windowOpenedPromise = BrowserTestUtils.waitForNewWindow();
  let win = OpenBrowserWindow({ private: true });
  await windowOpenedPromise;
  let panel = win.document
    .getElementById("extension-new-tab-notification")
    .closest("panel");
  let popupShown = promisePopupShown(panel);
  win.BrowserOpenTab();
  await popupShown;

  let url = await extension.awaitMessage("from-newtab-page");
  ok(url.endsWith("newtab.html"), "Newtab url is overridden by the extension.");

  // This will show a confirmation doorhanger, make sure we don't leave it open.
  let popupHidden = promisePopupHidden(panel);
  panel.hidePopup();
  await popupHidden;

  await extension.unload();
  await BrowserTestUtils.closeWindow(win);
});

// Test that prefs set by the newtab override code are
// properly unset when all newtab extensions are gone.
add_task(async function testNewTabPrefsReset() {
  function isUndefinedPref(pref) {
    try {
      Services.prefs.getBoolPref(pref);
      return false;
    } catch (e) {
      return true;
    }
  }

  ok(
    isUndefinedPref("browser.newtab.extensionControlled"),
    "extensionControlled pref is not set"
  );
  ok(
    isUndefinedPref("browser.newtab.privateAllowed"),
    "privateAllowed pref is not set"
  );
});
