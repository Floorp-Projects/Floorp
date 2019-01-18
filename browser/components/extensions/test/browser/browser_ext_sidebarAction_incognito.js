/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function testIncognito(incognitoOverride) {
  SpecialPowers.pushPrefEnv({set: [
    ["extensions.allowPrivateBrowsingByDefault", false],
  ]});

  let extension = ExtensionTestUtils.loadExtension({
    incognitoOverride,
    manifest: {
      sidebar_action: {
        default_panel: "sidebar.html",
      },
    },
    background() {
      browser.test.onMessage.addListener(async pbw => {
        await browser.test.assertRejects(browser.sidebarAction.setTitle({
          windowId: pbw.windowId, title: "test",
        }), /Invalid window ID/, "should not be able to set title");
        await browser.test.assertRejects(browser.sidebarAction.setTitle({
          tabId: pbw.tabId, title: "test",
        }), /Invalid tab ID/, "should not be able to set title");
        await browser.test.assertRejects(browser.sidebarAction.getTitle({
          windowId: pbw.windowId,
        }), /Invalid window ID/, "should not be able to get title");
        await browser.test.assertRejects(browser.sidebarAction.getTitle({
          tabId: pbw.tabId,
        }), /Invalid tab ID/, "should not be able to get title");

        await browser.test.assertRejects(browser.sidebarAction.setIcon({
          windowId: pbw.windowId, path: "test",
        }), /Invalid window ID/, "should not be able to set icon");
        await browser.test.assertRejects(browser.sidebarAction.setIcon({
          tabId: pbw.tabId, path: "test",
        }), /Invalid tab ID/, "should not be able to set icon");

        await browser.test.assertRejects(browser.sidebarAction.setPanel({
          windowId: pbw.windowId, panel: "test",
        }), /Invalid window ID/, "should not be able to set panel");
        await browser.test.assertRejects(browser.sidebarAction.setPanel({
          tabId: pbw.tabId, panel: "test",
        }), /Invalid tab ID/, "should not be able to set panel");
        await browser.test.assertRejects(browser.sidebarAction.getPanel({
          windowId: pbw.windowId,
        }), /Invalid window ID/, "should not be able to get panel");
        await browser.test.assertRejects(browser.sidebarAction.getPanel({
          tabId: pbw.tabId,
        }), /Invalid tab ID/, "should not be able to get panel");

        await browser.test.assertRejects(browser.sidebarAction.isOpen({
          windowId: pbw.windowId,
        }), /Invalid window ID/, "should not be able to determine openness");

        browser.test.notifyPass("pass");
      });
    },
    files: {
      "sidebar.html": `
        <!DOCTYPE html>
        <html>
        <head><meta charset="utf-8"/>
        <script src="sidebar.js"></script>
        </head>
        <body>
          A Test Sidebar
        </body>
        </html>
      `,

      "sidebar.js": function() {
        window.onload = () => {
          browser.test.sendMessage("sidebar");
        };
      },
    },
  });

  await extension.startup();
  let sidebarID = `${makeWidgetId(extension.id)}-sidebar-action`;
  ok(SidebarUI.sidebars.has(sidebarID), "sidebar exists in non-private window");

  let winData = await getIncognitoWindow("about:blank");

  let hasSidebar = winData.win.SidebarUI.sidebars.has(sidebarID);
  if (incognitoOverride == "spanning") {
    ok(hasSidebar, "sidebar exists in private window");
  } else {
    ok(!hasSidebar, "sidebar does not exist in private window");
    // Test API access to private window data.
    extension.sendMessage(winData.details);
    await extension.awaitFinish("pass");
  }

  await BrowserTestUtils.closeWindow(winData.win);
  await extension.unload();
}

add_task(async function test_sidebarAction_not_allowed() {
  await testIncognito();
});

add_task(async function test_sidebarAction_allowed() {
  await testIncognito("spanning");
});
