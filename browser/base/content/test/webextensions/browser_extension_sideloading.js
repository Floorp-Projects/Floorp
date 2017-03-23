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
    AddonManagerPrivate.callAddonListeners(val ? "onDisabled" : "onEnabled", this);
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

function promiseSetDisabled(addon) {
  return new Promise(resolve => {
    setCallbacks.set(addon, resolve);
  });
}

let cleanup;

add_task(function* () {
  // ICON_URL wouldn't ever appear as an actual webextension icon, but
  // we're just mocking out the addon here, so all we care about is that
  // that it propagates correctly to the popup.
  const ICON_URL = "chrome://mozapps/skin/extensions/category-extensions.svg";
  const DEFAULT_ICON_URL = "chrome://mozapps/skin/extensions/extensionGeneric.svg";

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
    iconURL: ICON_URL,
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

  const ID3 = "addon3@tests.mozilla.org";
  let mock3 = new MockAddon({
    id: ID3,
    name: "Test 3",
    isWebExtension: true,
    userDisabled: true,
    seen: false,
    userPermissions: {
      permissions: [],
      hosts: ["<all_urls>"],
    }
  });

  const ID4 = "addon4@tests.mozilla.org";
  let mock4 = new MockAddon({
    id: ID4,
    name: "Test 4",
    isWebExtension: true,
    userDisabled: true,
    seen: false,
    userPermissions: {
      permissions: [],
      hosts: ["<all_urls>"],
    }
  });

  let provider = new MockProvider(mock1, mock2, mock3, mock4);
  AddonManagerPrivate.registerProvider(provider, [{
    id: "extension",
    name: "Extensions",
    uiPriority: 4000,
    flags: AddonManager.TYPE_UI_VIEW_LIST |
           AddonManager.TYPE_SUPPORTS_UNDO_RESTARTLESS_UNINSTALL,
  }]);

  testCleanup = async function() {
    AddonManagerPrivate.unregisterProvider(provider);

    // clear out ExtensionsUI state about sideloaded extensions so
    // subsequent tests don't get confused.
    ExtensionsUI.sideloaded.clear();
    ExtensionsUI.emit("change");
  };

  // Navigate away from the starting page to force about:addons to load
  // in a new tab during the tests below.
  gBrowser.selectedBrowser.loadURI("about:robots");
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  registerCleanupFunction(function*() {
    // Return to about:blank when we're done
    gBrowser.selectedBrowser.loadURI("about:blank");
    yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  });

  hookExtensionsTelemetry();

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
  is(addons.children.length, 4, "Have 4 menu entries for sideloaded extensions");

  // Click the first sideloaded extension
  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  addons.children[0].click();

  // When we get the permissions prompt, we should be at the extensions
  // list in about:addons
  let panel = yield popupPromise;
  is(gBrowser.currentURI.spec, "about:addons", "Foreground tab is at about:addons");

  const VIEW = "addons://list/extension";
  let win = gBrowser.selectedBrowser.contentWindow;
  ok(!win.gViewController.isLoading, "about:addons view is fully loaded");
  is(win.gViewController.currentViewId, VIEW, "about:addons is at extensions list");

  // Check the contents of the notification, then choose "Cancel"
  checkNotification(panel, ICON_URL, [
    ["webextPerms.hostDescription.allUrls"],
    ["webextPerms.description.history"],
  ]);

  let disablePromise = promiseSetDisabled(mock1);
  panel.secondaryButton.click();

  let value = yield disablePromise;
  is(value, true, "Addon should remain disabled");

  let [addon1, addon2, addon3, addon4] = yield AddonManager.getAddonsByIDs([ID1, ID2, ID3, ID4]);
  ok(addon1.seen, "Addon should be marked as seen");
  is(addon1.userDisabled, true, "Addon 1 should still be disabled");
  is(addon2.userDisabled, true, "Addon 2 should still be disabled");
  is(addon3.userDisabled, true, "Addon 3 should still be disabled");
  is(addon4.userDisabled, true, "Addon 4 should still be disabled");

  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // Should still have 3 entries in the hamburger menu
  yield PanelUI.show();

  addons = document.getElementById("PanelUI-footer-addons");
  is(addons.children.length, 3, "Have 3 menu entries for sideloaded extensions");

  // Click the second sideloaded extension and wait for the notification
  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  addons.children[0].click();
  panel = yield popupPromise;

  // Again we should be at the extentions list in about:addons
  is(gBrowser.currentURI.spec, "about:addons", "Foreground tab is at about:addons");

  win = gBrowser.selectedBrowser.contentWindow;
  ok(!win.gViewController.isLoading, "about:addons view is fully loaded");
  is(win.gViewController.currentViewId, VIEW, "about:addons is at extensions list");

  // Check the notification contents.
  checkNotification(panel, DEFAULT_ICON_URL, []);

  // This time accept the install.
  disablePromise = promiseSetDisabled(mock2);
  panel.button.click();

  value = yield disablePromise;
  is(value, false, "Addon should be set to enabled");

  [addon1, addon2, addon3, addon4] = yield AddonManager.getAddonsByIDs([ID1, ID2, ID3, ID4]);
  is(addon1.userDisabled, true, "Addon 1 should still be disabled");
  is(addon2.userDisabled, false, "Addon 2 should now be enabled");
  is(addon3.userDisabled, true, "Addon 3 should still be disabled");
  is(addon4.userDisabled, true, "Addon 4 should still be disabled");

  // Should still have 2 entries in the hamburger menu
  yield PanelUI.show();

  addons = document.getElementById("PanelUI-footer-addons");
  is(addons.children.length, 2, "Have 2 menu entries for sideloaded extensions");

  // Close the hamburger menu and go directly to the addons manager
  yield PanelUI.hide();

  win = yield BrowserOpenAddonsMgr(VIEW);

  let list = win.document.getElementById("addon-list");

  // Make sure XBL bindings are applied
  list.clientHeight;

  let item = list.children.find(_item => _item.value == ID3);
  ok(item, "Found entry for sideloaded extension in about:addons");
  item.scrollIntoView({behavior: "instant"});

  ok(is_visible(item._enableBtn), "Enable button is visible for sideloaded extension");
  ok(is_hidden(item._disableBtn), "Disable button is not visible for sideloaded extension");

  // When clicking enable we should see the permissions notification
  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  BrowserTestUtils.synthesizeMouseAtCenter(item._enableBtn, {},
                                           gBrowser.selectedBrowser);
  panel = yield popupPromise;
  checkNotification(panel, DEFAULT_ICON_URL, [["webextPerms.hostDescription.allUrls"]]);

  // Accept the permissions
  disablePromise = promiseSetDisabled(mock3);
  panel.button.click();
  value = yield disablePromise;
  is(value, false, "userDisabled should be set on addon 3");

  addon3 = yield AddonManager.getAddonByID(ID3);
  is(addon3.userDisabled, false, "Addon 3 should be enabled");

  // Should still have 1 entry in the hamburger menu
  yield PanelUI.show();

  addons = document.getElementById("PanelUI-footer-addons");
  is(addons.children.length, 1, "Have 1 menu entry for sideloaded extensions");

  // Close the hamburger menu and go to the detail page for this addon
  yield PanelUI.hide();

  win = yield BrowserOpenAddonsMgr(`addons://detail/${encodeURIComponent(ID4)}`);
  let button = win.document.getElementById("detail-enable-btn");

  // When clicking enable we should see the permissions notification
  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  BrowserTestUtils.synthesizeMouseAtCenter(button, {},
                                           gBrowser.selectedBrowser);
  panel = yield popupPromise;
  checkNotification(panel, DEFAULT_ICON_URL, [["webextPerms.hostDescription.allUrls"]]);

  // Accept the permissions
  disablePromise = promiseSetDisabled(mock4);
  panel.button.click();
  value = yield disablePromise;
  is(value, false, "userDisabled should be set on addon 4");

  addon4 = yield AddonManager.getAddonByID(ID4);
  is(addon4.userDisabled, false, "Addon 4 should be enabled");

  // We should have recorded 1 cancelled followed by 3 accepted sideloads.
  expectTelemetry(["sideloadRejected", "sideloadAccepted", "sideloadAccepted", "sideloadAccepted"]);

  isnot(menuButton.getAttribute("badge-status"), "addon-alert", "Should no longer have addon alert badge");

  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
