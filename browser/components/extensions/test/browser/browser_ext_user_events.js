"use strict";

/**
 * Wait for the given PopupNotification to display
 *
 * @param {string} name
 *        The name of the notification to wait for.
 *
 * @returns {Promise}
 *          Resolves with the notification window.
 */
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

// Test that different types of events are all considered
// "handling user input".
add_task(async function testSources() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      async function request(perm) {
        try {
          let result = await browser.permissions.request({
            permissions: [perm],
          });
          browser.test.sendMessage("request", {success: true, result});
        } catch (err) {
          browser.test.sendMessage("request", {success: false, errmsg: err.message});
        }
      }

      let tabs = await browser.tabs.query({active: true, currentWindow: true});
      await browser.pageAction.show(tabs[0].id);

      browser.pageAction.onClicked.addListener(() => request("bookmarks"));
      browser.browserAction.onClicked.addListener(() => request("tabs"));

      browser.contextMenus.create({
        id: "menu",
        title: "test user events",
        contexts: ["page"],
      });
      browser.contextMenus.onClicked.addListener(() => request("webNavigation"));

      browser.test.sendMessage("actions-ready");
    },

    manifest: {
      browser_action: {default_title: "test"},
      page_action: {default_title: "test"},
      permissions: ["contextMenus"],
      optional_permissions: ["bookmarks", "tabs", "webNavigation"],
    },
  });

  async function check(what) {
    let result = await extension.awaitMessage("request");
    ok(result.success, `request() did not throw when called from ${what}`);
    is(result.result, true, `request() succeeded when called from ${what}`);
  }

  // Remove Sidebar button to prevent pushing extension button to overflow menu
  CustomizableUI.removeWidgetFromArea("sidebar-button");

  await extension.startup();
  await extension.awaitMessage("actions-ready");

  promisePopupNotificationShown("addon-webext-permissions").then(panel => {
    panel.button.click();
  });

  clickPageAction(extension);
  await check("page action click");

  promisePopupNotificationShown("addon-webext-permissions").then(panel => {
    panel.button.click();
  });

  clickBrowserAction(extension);
  await check("browser action click");

  promisePopupNotificationShown("addon-webext-permissions").then(panel => {
    panel.button.click();
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  gBrowser.selectedTab = tab;

  let menu = await openContextMenu("body");
  let items = menu.getElementsByAttribute("label", "test user events");
  is(items.length, 1, "Found context menu item");
  EventUtils.synthesizeMouseAtCenter(items[0], {});
  await check("context menu click");

  BrowserTestUtils.removeTab(tab);

  await extension.unload();

  registerCleanupFunction(() => CustomizableUI.reset());
});

