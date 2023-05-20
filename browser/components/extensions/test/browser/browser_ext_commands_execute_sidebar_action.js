/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_execute_sidebar_action() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      commands: {
        _execute_sidebar_action: {
          suggested_key: {
            default: "Alt+Shift+J",
          },
        },
      },
      sidebar_action: {
        default_panel: "sidebar.html",
      },
    },
    // We want to confirm that sidebar is not shown on every extension start,
    // so we use an explicit APP_STARTUP.
    startupReason: "APP_STARTUP",
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

      "sidebar.js": function () {
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

  await extension.startup();
  await SimpleTest.promiseFocus(window);
  ok(
    document.getElementById("sidebar-box").hidden,
    `Sidebar box is not visible after "not-first" startup.`
  );
  // Send the key to open the sidebar.
  EventUtils.synthesizeKey("j", { altKey: true, shiftKey: true });
  await extension.awaitFinish("execute-sidebar-action-opened");
  await extension.unload();
});
