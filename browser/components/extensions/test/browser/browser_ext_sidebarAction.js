/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

requestLongerTimeout(2);

let extData = {
  manifest: {
    sidebar_action: {
      default_panel: "sidebar.html",
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
    browser.test.onMessage.addListener(async ({ msg, data }) => {
      if (msg === "set-panel") {
        await browser.sidebarAction.setPanel({ panel: null });
        browser.test.assertEq(
          await browser.sidebarAction.getPanel({}),
          browser.runtime.getURL("sidebar.html"),
          "Global panel can be reverted to the default."
        );
      } else if (msg === "isOpen") {
        let { arg = {}, result } = data;
        let isOpen = await browser.sidebarAction.isOpen(arg);
        browser.test.assertEq(result, isOpen, "expected value from isOpen");
      }
      browser.test.sendMessage("done");
    });
  },
};

function getExtData(manifestUpdates = {}) {
  return {
    ...extData,
    manifest: {
      ...extData.manifest,
      ...manifestUpdates,
    },
  };
}

async function sendMessage(ext, msg, data = undefined) {
  ext.sendMessage({ msg, data });
  await ext.awaitMessage("done");
}

add_task(async function sidebar_initial_install() {
  ok(
    document.getElementById("sidebar-box").hidden,
    "sidebar box is not visible"
  );
  let extension = ExtensionTestUtils.loadExtension(getExtData());
  await extension.startup();
  await extension.awaitMessage("sidebar");

  // Test sidebar is opened on install
  ok(!document.getElementById("sidebar-box").hidden, "sidebar box is visible");

  await extension.unload();
  // Test that the sidebar was closed on unload.
  ok(
    document.getElementById("sidebar-box").hidden,
    "sidebar box is not visible"
  );
});

add_task(async function sidebar__install_closed() {
  ok(
    document.getElementById("sidebar-box").hidden,
    "sidebar box is not visible"
  );
  let tempExtData = getExtData();
  tempExtData.manifest.sidebar_action.open_at_install = false;
  let extension = ExtensionTestUtils.loadExtension(tempExtData);
  await extension.startup();

  // Test sidebar is closed on install
  ok(document.getElementById("sidebar-box").hidden, "sidebar box is hidden");

  await extension.unload();
  // This is the default value
  tempExtData.manifest.sidebar_action.open_at_install = true;
});

add_task(async function sidebar_two_sidebar_addons() {
  let extension2 = ExtensionTestUtils.loadExtension(getExtData());
  await extension2.startup();
  // Test sidebar is opened on install
  await extension2.awaitMessage("sidebar");
  ok(!document.getElementById("sidebar-box").hidden, "sidebar box is visible");

  // Test second sidebar install opens new sidebar
  let extension3 = ExtensionTestUtils.loadExtension(getExtData());
  await extension3.startup();
  // Test sidebar is opened on install
  await extension3.awaitMessage("sidebar");
  ok(!document.getElementById("sidebar-box").hidden, "sidebar box is visible");
  await extension3.unload();

  // We just close the sidebar on uninstall of the current sidebar.
  ok(
    document.getElementById("sidebar-box").hidden,
    "sidebar box is not visible"
  );

  await extension2.unload();
});

add_task(async function sidebar_empty_panel() {
  let extension = ExtensionTestUtils.loadExtension(getExtData());
  await extension.startup();
  // Test sidebar is opened on install
  await extension.awaitMessage("sidebar");
  ok(
    !document.getElementById("sidebar-box").hidden,
    "sidebar box is visible in first window"
  );
  await sendMessage(extension, "set-panel");
  await extension.unload();
});

add_task(async function sidebar_isOpen() {
  info("Load extension1");
  let extension1 = ExtensionTestUtils.loadExtension(getExtData());
  await extension1.startup();

  info("Test extension1's sidebar is opened on install");
  await extension1.awaitMessage("sidebar");
  await sendMessage(extension1, "isOpen", { result: true });
  let sidebar1ID = SidebarUI.currentID;

  info("Load extension2");
  let extension2 = ExtensionTestUtils.loadExtension(getExtData());
  await extension2.startup();

  info("Test extension2's sidebar is opened on install");
  await extension2.awaitMessage("sidebar");
  await sendMessage(extension1, "isOpen", { result: false });
  await sendMessage(extension2, "isOpen", { result: true });

  info("Switch back to extension1's sidebar");
  SidebarUI.show(sidebar1ID);
  await extension1.awaitMessage("sidebar");
  await sendMessage(extension1, "isOpen", { result: true });
  await sendMessage(extension2, "isOpen", { result: false });

  info("Test passing a windowId parameter");
  let windowId = window.docShell.outerWindowID;
  let WINDOW_ID_CURRENT = -2;
  await sendMessage(extension1, "isOpen", { arg: { windowId }, result: true });
  await sendMessage(extension2, "isOpen", { arg: { windowId }, result: false });
  await sendMessage(extension1, "isOpen", {
    arg: { windowId: WINDOW_ID_CURRENT },
    result: true,
  });
  await sendMessage(extension2, "isOpen", {
    arg: { windowId: WINDOW_ID_CURRENT },
    result: false,
  });

  info("Open a new window");
  open("", "", "noopener");
  let newWin = Services.wm.getMostRecentWindow("navigator:browser");

  info("The new window has no sidebar");
  await sendMessage(extension1, "isOpen", { result: false });
  await sendMessage(extension2, "isOpen", { result: false });

  info("But the original window still does");
  await sendMessage(extension1, "isOpen", { arg: { windowId }, result: true });
  await sendMessage(extension2, "isOpen", { arg: { windowId }, result: false });

  info("Close the new window");
  newWin.close();

  info("Close the sidebar in the original window");
  SidebarUI.hide();
  await sendMessage(extension1, "isOpen", { result: false });
  await sendMessage(extension2, "isOpen", { result: false });

  await extension1.unload();
  await extension2.unload();
});

add_task(async function testShortcuts() {
  function verifyShortcut(id, commandKey) {
    // We're just testing the command key since the modifiers have different
    // icons on different platforms.
    let button = document.getElementById(
      `button_${makeWidgetId(id)}-sidebar-action`
    );
    ok(button.hasAttribute("key"), "The menu item has a key specified");
    let key = document.getElementById(button.getAttribute("key"));
    ok(key, "The key attribute finds the related key element");
    ok(
      button.getAttribute("shortcut").endsWith(commandKey),
      "The shortcut has the right key"
    );
  }

  let extension1 = ExtensionTestUtils.loadExtension(
    getExtData({
      commands: {
        _execute_sidebar_action: {
          suggested_key: {
            default: "Ctrl+Shift+I",
          },
        },
      },
    })
  );
  let extension2 = ExtensionTestUtils.loadExtension(
    getExtData({
      commands: {
        _execute_sidebar_action: {
          suggested_key: {
            default: "Ctrl+Shift+E",
          },
        },
      },
    })
  );

  await extension1.startup();
  await extension1.awaitMessage("sidebar");

  // Open and close the switcher panel to trigger shortcut content rendering.
  let switcherPanelShown = promisePopupShown(SidebarUI._switcherPanel);
  SidebarUI.showSwitcherPanel();
  await switcherPanelShown;
  let switcherPanelHidden = promisePopupHidden(SidebarUI._switcherPanel);
  SidebarUI.hideSwitcherPanel();
  await switcherPanelHidden;

  // Test that the key is set for the extension after the shortcuts are rendered.
  verifyShortcut(extension1.id, "I");

  await extension2.startup();
  await extension2.awaitMessage("sidebar");

  // Once the switcher panel has been opened new shortcuts should be added
  // automatically.
  verifyShortcut(extension2.id, "E");

  await extension1.unload();
  await extension2.unload();
});
