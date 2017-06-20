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
    browser.test.onMessage.addListener(msg => {
      if (msg === "set-panel") {
        browser.sidebarAction.setPanel({panel: ""}).then(() => {
          browser.test.notifyFail("empty panel settable");
        }).catch(() => {
          browser.test.notifyPass("unable to set empty panel");
        });
      }
    });
  },
};

add_task(async function sidebar_initial_install() {
  ok(document.getElementById("sidebar-box").hidden, "sidebar box is not visible");
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();
  // Test sidebar is opened on install
  await extension.awaitMessage("sidebar");
  ok(!document.getElementById("sidebar-box").hidden, "sidebar box is visible");
  // Test toolbar button is available
  ok(document.getElementById("sidebar-button"), "sidebar button is in UI");

  await extension.unload();
  // Test that the sidebar was closed on unload.
  ok(document.getElementById("sidebar-box").hidden, "sidebar box is not visible");

  // Move toolbar button back to customization.
  CustomizableUI.removeWidgetFromArea("sidebar-button", CustomizableUI.AREA_NAVBAR);
  ok(!document.getElementById("sidebar-button"), "sidebar button is not in UI");
});


add_task(async function sidebar_two_sidebar_addons() {
  let extension2 = ExtensionTestUtils.loadExtension(extData);
  await extension2.startup();
  // Test sidebar is opened on install
  await extension2.awaitMessage("sidebar");
  ok(!document.getElementById("sidebar-box").hidden, "sidebar box is visible");
  // Test toolbar button is NOT available after first install
  ok(!document.getElementById("sidebar-button"), "sidebar button is not in UI");

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
  extension.sendMessage("set-panel");
  await extension.awaitFinish();
  await extension.unload();
});

add_task(async function cleanup() {
  // This is set on initial sidebar install.
  Services.prefs.clearUserPref("extensions.sidebar-button.shown");
});
