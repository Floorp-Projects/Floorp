/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

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
};

add_task(async function sidebar_windows() {
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();
  // Test sidebar is opened on install
  await extension.awaitMessage("sidebar");
  ok(!document.getElementById("sidebar-box").hidden, "sidebar box is visible in first window");
  // Check that the menuitem has our image styling.
  let elements = document.getElementsByClassName("webextension-menuitem");
  // ui is in flux, at time of writing we potentially have 3 menuitems, later
  // it may be two or one, just make sure one is there.
  ok(elements.length > 0, "have a menuitem");
  let style = elements[0].getAttribute("style");
  ok(style.includes("webextension-menuitem-image"), "this menu has style");

  let secondSidebar = extension.awaitMessage("sidebar");

  // SidebarUI relies on window.opener being set, which is normal behavior when
  // using menu or key commands to open a new browser window.
  let win = await BrowserTestUtils.openNewBrowserWindow({opener: window});

  await secondSidebar;
  ok(!win.document.getElementById("sidebar-box").hidden, "sidebar box is visible in second window");
  // Check that the menuitem has our image styling.
  elements = win.document.getElementsByClassName("webextension-menuitem");
  ok(elements.length > 0, "have a menuitem");
  style = elements[0].getAttribute("style");
  ok(style.includes("webextension-menuitem-image"), "this menu has style");

  await extension.unload();
  await BrowserTestUtils.closeWindow(win);

  // Move toolbar button back to customization.
  CustomizableUI.removeWidgetFromArea("sidebar-button", CustomizableUI.AREA_NAVBAR);
  ok(!document.getElementById("sidebar-button"), "sidebar button is not in UI");
  // This is set on initial sidebar install.
  Services.prefs.clearUserPref("extensions.sidebar-button.shown");
});
