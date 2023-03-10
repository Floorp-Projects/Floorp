/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

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
};

add_task(async function sidebar_windows() {
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();
  // Test sidebar is opened on install
  await extension.awaitMessage("sidebar");
  ok(
    !document.getElementById("sidebar-box").hidden,
    "sidebar box is visible in first window"
  );
  // Check that the toolbarbutton has our image styling.
  let button = document.querySelector(".sidebar-extensions-subviewbutton");
  ok(!!button, "have a toolbarbutton");
  let style = button.getAttribute("style");
  ok(
    style.includes("webextension-sidebar-subviewbutton-image"),
    "this button has our style"
  );

  let secondSidebar = extension.awaitMessage("sidebar");

  // SidebarUI relies on window.opener being set, which is normal behavior when
  // using menu or key commands to open a new browser window.
  let win = await BrowserTestUtils.openNewBrowserWindow();

  await secondSidebar;
  ok(
    !win.document.getElementById("sidebar-box").hidden,
    "sidebar box is visible in second window"
  );

  button = win.document.querySelector(".sidebar-extensions-subviewbutton");
  ok(!!button, "have a toolbarbutton");
  style = button.getAttribute("style");
  ok(
    style.includes("webextension-sidebar-subviewbutton-image"),
    "this button has our style"
  );

  await extension.unload();
  await BrowserTestUtils.closeWindow(win);
});
