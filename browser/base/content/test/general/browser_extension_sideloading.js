const {AddonManagerPrivate} = Cu.import("resource://gre/modules/AddonManager.jsm", {});

// MockAddon mimics the AddonInternal interface and MockProvider implements
// just enough of the AddonManager provider interface to make it look like
// we have sideloaded webextensions so the sideloading flow can be tested.

// MockAddon -> callback
let setCallbacks = new Map();

class MockAddon {
  constructor(props) {
    this._userDisabled = false;
    this.pendingOperations = 0;
    this.type = "extension";

    for (let name in props) {
      if (name == "userDisabled") {
        this._userDisabled = props[name];
      }
      this[name] = props[name];
    }
  }

  markAsSeen() {
    this.seen = true;
  }

  get userDisabled() {
    return this._userDisabled;
  }

  set userDisabled(val) {
    this._userDisabled = val;
    let fn = setCallbacks.get(this);
    if (fn) {
      setCallbacks.delete(this);
      fn(val);
    }
    return val;
  }

  get permissions() {
    return this._userDisabled ? AddonManager.PERM_CAN_ENABLE : AddonManager.PERM_CAN_DISABLE;
  }
}

class MockProvider {
  constructor(...addons) {
    this.addons = new Set(addons);
  }

  startup() { }
  shutdown() { }

  getAddonByID(id, callback) {
    for (let addon of this.addons) {
      if (addon.id == id) {
        callback(addon);
        return;
      }
    }
    callback(null);
  }

  getAddonsByTypes(types, callback) {
    let addons = [];
    if (!types || types.includes("extension")) {
      addons = [...this.addons];
    }
    callback(addons);
  }
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
      if (!notification) {
        return;
      }

      ok(notification, `${name} notification shown`);
      ok(PopupNotifications.isPanelOpen, "notification panel open");

      PopupNotifications.panel.removeEventListener("popupshown", popupshown);
      resolve(PopupNotifications.panel.firstChild);
    }

    PopupNotifications.panel.addEventListener("popupshown", popupshown);
  });
}

function promiseSetDisabled(addon) {
  return new Promise(resolve => {
    setCallbacks.set(addon, resolve);
  });
}

add_task(function* () {
  // XXX remove this when prompts are enabled by default
  yield SpecialPowers.pushPrefEnv({set: [
    ["extensions.webextPermissionPrompts", true],
  ]});

  const ID1 = "addon1@tests.mozilla.org";
  let mock1 = new MockAddon({
    id: ID1,
    name: "Test 1",
    userDisabled: true,
    seen: false,
    userPermissions: {
      permissions: ["history"],
      hosts: ["https://*/*"],
    },
  });

  const ID2 = "addon2@tests.mozilla.org";
  let mock2 = new MockAddon({
    id: ID2,
    name: "Test 2",
    userDisabled: true,
    seen: false,
    userPermissions: {
      permissions: [],
      hosts: [],
    },
  });

  let provider = new MockProvider(mock1, mock2);
  AddonManagerPrivate.registerProvider(provider, [{
    id: "extension",
    name: "Extensions",
    uiPriority: 4000,
    flags: AddonManager.TYPE_UI_VIEW_LIST |
           AddonManager.TYPE_SUPPORTS_UNDO_RESTARTLESS_UNINSTALL,
  }]);
  registerCleanupFunction(function*() {
    AddonManagerPrivate.unregisterProvider(provider);

    // clear out ExtensionsUI state about sideloaded extensions so
    // subsequent tests don't get confused.
    ExtensionsUI.sideloaded.clear();
    ExtensionsUI.emit("change");
  });

  let changePromise = new Promise(resolve => {
    ExtensionsUI.on("change", function listener() {
      ExtensionsUI.off("change", listener);
      resolve();
    });
  });
  ExtensionsUI._checkForSideloaded();
  yield changePromise;

  // Check for the addons badge on the hamburger menu
  let menuButton = document.getElementById("PanelUI-menu-button");
  is(menuButton.getAttribute("badge-status"), "addon-alert", "Should have addon alert badge");

  // Find the menu entries for sideloaded extensions
  yield PanelUI.show();

  let addons = document.getElementById("PanelUI-footer-addons");
  is(addons.children.length, 2, "Have 2 menu entries for sideloaded extensions");

  // Click the first sideloaded extension
  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:addons");
  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  addons.children[0].click();

  // about:addons should load and go to the list of extensions
  let tab = yield tabPromise;
  is(tab.linkedBrowser.currentURI.spec, "about:addons", "Newly opened tab is at about:addons");

  const VIEW = "addons://list/extension";
  yield promiseViewLoaded(tab, VIEW);
  let win = tab.linkedBrowser.contentWindow;
  ok(!win.gViewController.isLoading, "about:addons view is fully loaded");
  is(win.gViewController.currentViewId, VIEW, "about:addons is at extensions list");

  // Wait for the permission prompt and cancel it
  let panel = yield popupPromise;
  let disablePromise = promiseSetDisabled(mock1);
  panel.secondaryButton.click();

  let value = yield disablePromise;
  is(value, true, "Addon should remain disabled");

  let [addon1, addon2] = yield AddonManager.getAddonsByIDs([ID1, ID2]);
  ok(addon1.seen, "Addon should be marked as seen");
  is(addon1.userDisabled, true, "Addon 1 should still be disabled");
  is(addon2.userDisabled, true, "Addon 2 should still be disabled");

  yield BrowserTestUtils.removeTab(tab);

  // Should still have 1 entry in the hamburger menu
  yield PanelUI.show();

  addons = document.getElementById("PanelUI-footer-addons");
  is(addons.children.length, 1, "Have 1 menu entry for sideloaded extensions");

  // Click the second sideloaded extension
  tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:addons");
  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  addons.children[0].click();

  tab = yield tabPromise;
  is(tab.linkedBrowser.currentURI.spec, "about:addons", "Newly opened tab is at about:addons");

  isnot(menuButton.getAttribute("badge-status"), "addon-alert", "Should no longer have addon alert badge");

  yield promiseViewLoaded(tab, VIEW);
  win = tab.linkedBrowser.contentWindow;
  ok(!win.gViewController.isLoading, "about:addons view is fully loaded");
  is(win.gViewController.currentViewId, VIEW, "about:addons is at extensions list");

  // Wait for the permission prompt and accept it this time
  panel = yield popupPromise;
  disablePromise = promiseSetDisabled(mock2);
  panel.button.click();

  value = yield disablePromise;
  is(value, false, "Addon should be set to enabled");

  [addon1, addon2] = yield AddonManager.getAddonsByIDs([ID1, ID2]);
  is(addon1.userDisabled, true, "Addon 1 should still be disabled");
  is(addon2.userDisabled, false, "Addon 2 should now be enabled");

  yield BrowserTestUtils.removeTab(tab);
});
