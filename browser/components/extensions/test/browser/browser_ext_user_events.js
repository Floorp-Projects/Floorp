"use strict";

// Test that different types of events are all considered
// "handling user input".
add_task(async function testSources() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      async function request() {
        try {
          let result = await browser.permissions.request({
            permissions: ["cookies"],
          });
          browser.test.sendMessage("request", {success: true, result});
        } catch (err) {
          browser.test.sendMessage("request", {success: false, errmsg: err.message});
        }
      }

      let tabs = await browser.tabs.query({active: true, currentWindow: true});
      await browser.pageAction.show(tabs[0].id);
      browser.test.sendMessage("page-action-shown");

      browser.pageAction.onClicked.addListener(request);
      browser.browserAction.onClicked.addListener(request);

      browser.contextMenus.create({
        id: "menu",
        title: "test user events",
        contexts: ["page"],
      });
      browser.contextMenus.onClicked.addListener(request);
    },

    manifest: {
      browser_action: {default_title: "test"},
      page_action: {default_title: "test"},
      permissions: ["contextMenus"],
      optional_permissions: ["cookies"],
    },
  });

  async function check(what) {
    let result = await extension.awaitMessage("request");
    ok(result.success, `request() did not throw when called from ${what}`);
    is(result.result, true, `request() succeeded when called from ${what}`);
  }

  await extension.startup();

  await extension.awaitMessage("page-action-shown");
  clickPageAction(extension);
  await check("page action click");

  clickBrowserAction(extension);
  await check("browser action click");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  gBrowser.selectedTab = tab;

  let menu = await openContextMenu("body");
  let items = menu.getElementsByAttribute("label", "test user events");
  is(items.length, 1, "Found context menu item");
  EventUtils.synthesizeMouseAtCenter(items[0], {});
  await check("context menu click");

  await BrowserTestUtils.removeTab(tab);

  await extension.unload();
});

