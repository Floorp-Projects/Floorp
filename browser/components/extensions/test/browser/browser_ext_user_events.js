"use strict";

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
          browser.test.sendMessage("request", { success: true, result, perm });
        } catch (err) {
          browser.test.sendMessage("request", {
            success: false,
            errmsg: err.message,
            perm,
          });
        }
      }

      let tabs = await browser.tabs.query({
        active: true,
        currentWindow: true,
      });
      await browser.pageAction.show(tabs[0].id);

      browser.pageAction.onClicked.addListener(() => request("bookmarks"));
      browser.browserAction.onClicked.addListener(() => request("tabs"));
      browser.commands.onCommand.addListener(() => request("downloads"));

      browser.test.onMessage.addListener(msg => {
        if (msg === "contextMenus.update") {
          browser.contextMenus.onClicked.addListener(() =>
            request("webNavigation")
          );
          browser.contextMenus.update(
            "menu",
            {
              title: "test user events in onClicked",
              onclick: null,
            },
            () => browser.test.sendMessage("contextMenus.update-done")
          );
        }
        if (msg === "openOptionsPage") {
          browser.runtime.openOptionsPage();
        }
      });

      browser.contextMenus.create(
        {
          id: "menu",
          title: "test user events in onclick",
          contexts: ["page"],
          onclick() {
            request("cookies");
          },
        },
        () => {
          browser.test.sendMessage("actions-ready");
        }
      );
    },

    files: {
      "options.html": `<!DOCTYPE html>
        <html lang="en">
        <head>
          <meta charset="UTF-8">
          <script src="options.js"></script>
          <script src="https://example.com/tests/SimpleTest/EventUtils.js"></script>
        </head>
        <body>
          <a id="link" href="#">Link</a>
        </body>
        </html>`,

      "options.js"() {
        addEventListener("load", async () => {
          let link = document.getElementById("link");
          link.onclick = async event => {
            link.onclick = null;
            event.preventDefault();

            browser.test.log("Calling permission.request from options page.");

            let perm = "history";
            try {
              let result = await browser.permissions.request({
                permissions: [perm],
              });
              browser.test.sendMessage("request", {
                success: true,
                result,
                perm,
              });
            } catch (err) {
              browser.test.sendMessage("request", {
                success: false,
                errmsg: err.message,
                perm,
              });
            }
          };

          // Make a few trips through the event loop to make sure the
          // options browser is fully visible. This is a bit dodgy, but
          // we don't really have a reliable way to detect this from the
          // options page side, and synthetic click events won't work
          // until it is.
          do {
            browser.test.log(
              "Waiting for the options browser to be visible..."
            );
            await new Promise(resolve => setTimeout(resolve, 0));
            synthesizeMouseAtCenter(link, {});
          } while (link.onclick !== null);
        });
      },
    },

    manifest: {
      browser_action: { default_title: "test" },
      page_action: { default_title: "test" },
      permissions: ["contextMenus"],
      optional_permissions: [
        "bookmarks",
        "tabs",
        "webNavigation",
        "history",
        "cookies",
        "downloads",
      ],
      options_ui: { page: "options.html" },
      content_security_policy:
        "script-src 'self' https://example.com; object-src 'none';",
      commands: {
        command: {
          suggested_key: {
            default: "Alt+Shift+J",
          },
        },
      },
    },

    useAddonManager: "temporary",
  });

  async function testPermissionRequest(
    { requestPermission, expectPrompt, perm },
    what
  ) {
    info(`check request permission from '${what}'`);

    let promptPromise = null;
    if (expectPrompt) {
      promptPromise = promisePopupNotificationShown(
        "addon-webext-permissions"
      ).then(panel => {
        panel.button.click();
      });
    }

    await requestPermission();
    await promptPromise;

    let result = await extension.awaitMessage("request");
    ok(result.success, `request() did not throw when called from ${what}`);
    is(result.result, true, `request() succeeded when called from ${what}`);
    is(result.perm, perm, `requested permission ${what}`);
    await promptPromise;
  }

  // Remove Sidebar button to prevent pushing extension button to overflow menu
  CustomizableUI.removeWidgetFromArea("sidebar-button");

  await extension.startup();
  await extension.awaitMessage("actions-ready");

  await testPermissionRequest(
    {
      requestPermission: () => clickPageAction(extension),
      expectPrompt: true,
      perm: "bookmarks",
    },
    "page action click"
  );

  await testPermissionRequest(
    {
      requestPermission: () => clickBrowserAction(extension),
      expectPrompt: true,
      perm: "tabs",
    },
    "browser action click"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  gBrowser.selectedTab = tab;

  await testPermissionRequest(
    {
      requestPermission: async () => {
        let menu = await openContextMenu("body");
        let items = menu.getElementsByAttribute(
          "label",
          "test user events in onclick"
        );
        is(items.length, 1, "Found context menu item");
        menu.activateItem(items[0]);
      },
      expectPrompt: false, // cookies permission has no prompt.
      perm: "cookies",
    },
    "context menu in onclick"
  );

  extension.sendMessage("contextMenus.update");
  await extension.awaitMessage("contextMenus.update-done");

  await testPermissionRequest(
    {
      requestPermission: async () => {
        let menu = await openContextMenu("body");
        let items = menu.getElementsByAttribute(
          "label",
          "test user events in onClicked"
        );
        is(items.length, 1, "Found context menu item again");
        menu.activateItem(items[0]);
      },
      expectPrompt: true,
      perm: "webNavigation",
    },
    "context menu in onClicked"
  );

  await testPermissionRequest(
    {
      requestPermission: () => {
        EventUtils.synthesizeKey("j", { altKey: true, shiftKey: true });
      },
      expectPrompt: true,
      perm: "downloads",
    },
    "commands shortcut"
  );

  await testPermissionRequest(
    {
      requestPermission: () => {
        extension.sendMessage("openOptionsPage");
      },
      expectPrompt: true,
      perm: "history",
    },
    "options page link click"
  );

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await BrowserTestUtils.removeTab(tab);

  await extension.unload();

  registerCleanupFunction(() => CustomizableUI.reset());
});
