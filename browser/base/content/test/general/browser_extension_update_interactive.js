const {AddonManagerPrivate} = Cu.import("resource://gre/modules/AddonManager.jsm", {});

const URL_BASE = "https://example.com/browser/browser/base/content/test/general";
const ID = "update2@tests.mozilla.org";
const ID_LEGACY = "legacy_update@tests.mozilla.org";

registerCleanupFunction(async function() {
  for (let id of [ID, ID_LEGACY]) {
    let addon = await AddonManager.getAddonByID(id);
    if (addon) {
      ok(false, `Addon ${id} was still installed at the end of the test`);
      addon.uninstall();
    }
  }
});

function promiseInstallAddon(url) {
  return AddonManager.getInstallForURL(url, null, "application/x-xpinstall")
                     .then(install => {
                       ok(install, "Created install");
                       return new Promise(resolve => {
                         install.addListener({
                           onInstallEnded(_install, addon) {
                             resolve(addon);
                           },
                         });
                         install.install();
                       });
                     });
}

function promiseViewLoaded(tab, viewid) {
  let win = tab.linkedBrowser.contentWindow;
  if (win.gViewController && !win.gViewController.isLoading &&
      win.gViewController.currentViewId == viewid) {
     return Promise.resolve();
  }

  return new Promise(resolve => {
    function listener() {
      if (win.gViewController.currentViewId != viewid) {
        return;
      }
      win.document.removeEventListener("ViewChanged", listener);
      resolve();
    }
    win.document.addEventListener("ViewChanged", listener);
  });
}

function promisePopupNotificationShown(name) {
  return new Promise(resolve => {
    function popupshown() {
      let notification = PopupNotifications.getNotification(name);
      if (!notification) { return; }

      ok(notification, `${name} notification shown`);
      ok(PopupNotifications.isPanelOpen, "notification panel open");

      PopupNotifications.panel.removeEventListener("popupshown", popupshown);
      resolve(PopupNotifications.panel.firstChild);
    }

    PopupNotifications.panel.addEventListener("popupshown", popupshown);
  });
}

function promiseInstallEvent(addon, event) {
  return new Promise(resolve => {
    let listener = {};
    listener[event] = (install, ...args) => {
      if (install.addon.id == addon.id) {
        AddonManager.removeInstallListener(listener);
        resolve(...args);
      }
    };
    AddonManager.addInstallListener(listener);
  });
}

// Set some prefs that apply to all the tests in this file
add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({set: [
    // We don't have pre-pinned certificates for the local mochitest server
    ["extensions.install.requireBuiltInCerts", false],
    ["extensions.update.requireBuiltInCerts", false],

    // XXX remove this when prompts are enabled by default
    ["extensions.webextPermissionPrompts", true],
  ]});
});

// Helper function to test a specific scenario for interactive updates.
// `checkFn` is a callable that triggers a check for updates.
// `autoUpdate` specifies whether the test should be run with
// updates applied automatically or not.
function* interactiveUpdateTest(autoUpdate, checkFn) {
  yield SpecialPowers.pushPrefEnv({set: [
    ["extensions.update.autoUpdateDefault", autoUpdate],

    // Point updates to the local mochitest server
    ["extensions.update.url", `${URL_BASE}/browser_webext_update.json`],
  ]});

  // Trigger an update check, manually applying the update if we're testing
  // without auto-update.
  function* triggerUpdate(win, addon) {
    let manualUpdatePromise;
    if (!autoUpdate) {
      manualUpdatePromise = new Promise(resolve => {
        let listener = {
          onNewInstall() {
            AddonManager.removeInstallListener(listener);
            resolve();
          },
        };
        AddonManager.addInstallListener(listener);
      });
    }

    checkFn(win, addon);

    if (manualUpdatePromise) {
      yield manualUpdatePromise;

      let list = win.document.getElementById("addon-list");

      // Make sure we have XBL bindings
      list.clientHeight;

      let item = list.children.find(_item => _item.value == ID);
      EventUtils.synthesizeMouseAtCenter(item._updateBtn, {}, win);
    }
  }

  // Install version 1.0 of the test extension
  let addon = yield promiseInstallAddon(`${URL_BASE}/browser_webext_update1.xpi`);
  ok(addon, "Addon was installed");
  is(addon.version, "1.0", "Version 1 of the addon is installed");

  // Open add-ons manager and navigate to extensions list
  let loadPromise = new Promise(resolve => {
    let listener = (subject, topic) => {
      if (subject.location.href == "about:addons") {
        Services.obs.removeObserver(listener, topic);
        resolve(subject);
      }
    };
    Services.obs.addObserver(listener, "EM-loaded", false);
  });
  let tab = gBrowser.addTab("about:addons");
  gBrowser.selectedTab = tab;
  let win = yield loadPromise;

  const VIEW = "addons://list/extension";
  let viewPromise = promiseViewLoaded(tab, VIEW);
  win.loadView(VIEW);
  yield viewPromise;

  // Trigger an update check
  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  yield triggerUpdate(win, addon);
  let panel = yield popupPromise;

  // Click the cancel button, wait to see the cancel event
  let cancelPromise = promiseInstallEvent(addon, "onInstallCancelled");
  panel.secondaryButton.click();
  yield cancelPromise;

  addon = yield AddonManager.getAddonByID(ID);
  is(addon.version, "1.0", "Should still be running the old version");

  // Trigger a new update check
  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  yield triggerUpdate(win, addon);

  // This time, accept the upgrade
  let updatePromise = promiseInstallEvent(addon, "onInstallEnded");
  panel = yield popupPromise;
  panel.button.click();

  addon = yield updatePromise;
  is(addon.version, "2.0", "Should have upgraded");

  yield BrowserTestUtils.removeTab(tab);
  addon.uninstall();
  yield SpecialPowers.popPrefEnv();
}

// Invoke the "Check for Updates" menu item
function checkAll(win) {
  win.gViewController.doCommand("cmd_findAllUpdates");
}

// Test "Check for Updates" with both auto-update settings
add_task(() => interactiveUpdateTest(true, checkAll));
add_task(() => interactiveUpdateTest(false, checkAll));


// Invoke an invidual extension's "Find Updates" menu item
function checkOne(win, addon) {
  win.gViewController.doCommand("cmd_findItemUpdates", addon);
}

// Test "Find Updates" with both auto-update settings
add_task(() => interactiveUpdateTest(true, checkOne));
add_task(() => interactiveUpdateTest(false, checkOne));

// Check that an update from a legacy extension to a webextensino
// does not display a prompt
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [
    // Point updates to the local mochitest server
    ["extensions.update.url", `${URL_BASE}/browser_webext_update.json`],
  ]});

  // Navigate away to ensure that BrowserOpenAddonMgr() opens a new tab
  gBrowser.selectedBrowser.loadURI("about:robots");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  // Install initial version of the test extension
  let addon = await promiseInstallAddon(`${URL_BASE}/browser_legacy.xpi`);
  ok(addon, "Addon was installed");
  is(addon.version, "1.1", "Version 1 of the addon is installed");

  // Go to Extensions in about:addons
  let win = await BrowserOpenAddonsMgr("addons://list/extension");

  let sawPopup = false;
  PopupNotifications.panel.addEventListener("popupshown",
                                            () => sawPopup = true,
                                            {once: true});

  // Trigger an update check, we should see the update get applied
  let updatePromise = promiseInstallEvent(addon, "onInstallEnded");
  win.gViewController.doCommand("cmd_findAllUpdates");
  await updatePromise;

  addon = await AddonManager.getAddonByID(ID_LEGACY);
  is(addon.version, "2.0", "Should have upgraded");

  ok(!sawPopup, "Should not have seen a permission notification");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  addon.uninstall();
  await SpecialPowers.popPrefEnv();
});
