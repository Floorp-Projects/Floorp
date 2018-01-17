/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

requestLongerTimeout(2);

let extData = {
  manifest: {
    "sidebar_action": {
      "default_panel": "sidebar.html",
    },
  },
  useAddonManager: "temporary",

  files: {
    "sidebar.html": `
      <!DOCTYPE html>
      <html>
      <head><meta charset="utf-8"/>
      <script src="sidebar.js"></script>
      </head>
      <body>
      A Test Sidebar
      </body></html>
    `,

    "sidebar.js": function() {
      window.onload = () => {
        browser.test.sendMessage("sidebar");
      };
    },
  },

  background: function() {
    browser.test.onMessage.addListener(async ({msg, data}) => {
      if (msg === "set-panel") {
        await browser.sidebarAction.setPanel({panel: ""}).then(() => {
          browser.test.notifyFail("empty panel settable");
        }).catch(() => {
          browser.test.notifyPass("unable to set empty panel");
        });
      } else if (msg === "isOpen") {
        let {arg = {}, result} = data;
        let isOpen = await browser.sidebarAction.isOpen(arg);
        browser.test.assertEq(result, isOpen, "expected value from isOpen");
      }
      browser.test.sendMessage("done");
    });
  },
};

async function sendMessage(ext, msg, data = undefined) {
  ext.sendMessage({msg, data});
  await ext.awaitMessage("done");
}

add_task(async function sidebar_initial_install() {
  ok(document.getElementById("sidebar-box").hidden, "sidebar box is not visible");
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();
  // Test sidebar is opened on install
  await extension.awaitMessage("sidebar");
  ok(!document.getElementById("sidebar-box").hidden, "sidebar box is visible");

  await extension.unload();
  // Test that the sidebar was closed on unload.
  ok(document.getElementById("sidebar-box").hidden, "sidebar box is not visible");
});


add_task(async function sidebar_two_sidebar_addons() {
  let extension2 = ExtensionTestUtils.loadExtension(extData);
  await extension2.startup();
  // Test sidebar is opened on install
  await extension2.awaitMessage("sidebar");
  ok(!document.getElementById("sidebar-box").hidden, "sidebar box is visible");

  // Test second sidebar install opens new sidebar
  let extension3 = ExtensionTestUtils.loadExtension(extData);
  await extension3.startup();
  // Test sidebar is opened on install
  await extension3.awaitMessage("sidebar");
  ok(!document.getElementById("sidebar-box").hidden, "sidebar box is visible");
  await extension3.unload();

  // We just close the sidebar on uninstall of the current sidebar.
  ok(document.getElementById("sidebar-box").hidden, "sidebar box is not visible");

  await extension2.unload();
});

add_task(async function sidebar_empty_panel() {
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();
  // Test sidebar is opened on install
  await extension.awaitMessage("sidebar");
  ok(!document.getElementById("sidebar-box").hidden, "sidebar box is visible in first window");
  await sendMessage(extension, "set-panel");
  await extension.awaitFinish();
  await extension.unload();
});

add_task(async function sidebar_isOpen() {
  info("Load extension1");
  let extension1 = ExtensionTestUtils.loadExtension(extData);
  await extension1.startup();

  info("Test extension1's sidebar is opened on install");
  await extension1.awaitMessage("sidebar");
  await sendMessage(extension1, "isOpen", {result: true});
  let sidebar1ID = SidebarUI.currentID;

  info("Load extension2");
  let extension2 = ExtensionTestUtils.loadExtension(extData);
  await extension2.startup();

  info("Test extension2's sidebar is opened on install");
  await extension2.awaitMessage("sidebar");
  await sendMessage(extension1, "isOpen", {result: false});
  await sendMessage(extension2, "isOpen", {result: true});

  info("Switch back to extension1's sidebar");
  SidebarUI.show(sidebar1ID);
  await extension1.awaitMessage("sidebar");
  await sendMessage(extension1, "isOpen", {result: true});
  await sendMessage(extension2, "isOpen", {result: false});

  info("Test passing a windowId parameter");
  let windowId = window.getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
  let WINDOW_ID_CURRENT = -2;
  await sendMessage(extension1, "isOpen", {arg: {windowId}, result: true});
  await sendMessage(extension2, "isOpen", {arg: {windowId}, result: false});
  await sendMessage(extension1, "isOpen", {arg: {windowId: WINDOW_ID_CURRENT}, result: true});
  await sendMessage(extension2, "isOpen", {arg: {windowId: WINDOW_ID_CURRENT}, result: false});

  info("Open a new window");
  let newWin = open();

  info("The new window has no sidebar");
  await sendMessage(extension1, "isOpen", {result: false});
  await sendMessage(extension2, "isOpen", {result: false});

  info("But the original window still does");
  await sendMessage(extension1, "isOpen", {arg: {windowId}, result: true});
  await sendMessage(extension2, "isOpen", {arg: {windowId}, result: false});

  info("Close the new window");
  newWin.close();

  info("Close the sidebar in the original window");
  SidebarUI.hide();
  await sendMessage(extension1, "isOpen", {result: false});
  await sendMessage(extension2, "isOpen", {result: false});

  await extension1.unload();
  await extension2.unload();
});
