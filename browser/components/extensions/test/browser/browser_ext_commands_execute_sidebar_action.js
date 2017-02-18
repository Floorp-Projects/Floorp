/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_execute_sidebar_action() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "commands": {
        "_execute_sidebar_action": {
          "suggested_key": {
            "default": "Alt+Shift+J",
          },
        },
      },
      "sidebar_action": {
        "default_panel": "sidebar.html",
      },
    },
    files: {
      "sidebar.html": `
        <!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="sidebar.js"></script>
          </head>
        </html>
      `,

      "sidebar.js": function() {
        browser.runtime.sendMessage("from-sidebar-action");
      },
    },
    background() {
      browser.runtime.onMessage.addListener(msg => {
        if (msg == "from-sidebar-action") {
          browser.test.notifyPass("execute-sidebar-action-opened");
        }
      });
    },
  });

  yield extension.startup();
  yield SimpleTest.promiseFocus(window);
  // Since we didn't set useAddonManager, the sidebar will not be automatically
  // opened for this test.
  ok(document.getElementById("sidebar-box").hidden, "sidebar box is not visible");
  // Send the key to open the sidebar.
  EventUtils.synthesizeKey("j", {altKey: true, shiftKey: true});
  yield extension.awaitFinish("execute-sidebar-action-opened");
  yield extension.unload();
});
