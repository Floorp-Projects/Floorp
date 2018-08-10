/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function sidebar_tab_query_bug_1340739() {
  let data = {
    manifest: {
      "permissions": [
        "tabs",
      ],
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
        Promise.all([
          browser.tabs.query({}).then((tabs) => {
            browser.test.assertEq(1, tabs.length, "got tab without currentWindow");
          }),
          browser.tabs.query({currentWindow: true}).then((tabs) => {
            browser.test.assertEq(1, tabs.length, "got tab with currentWindow");
          }),
        ]).then(() => {
          browser.test.sendMessage("sidebar");
        });
      },
    },
  };

  let extension = ExtensionTestUtils.loadExtension(data);
  await extension.startup();
  await extension.awaitMessage("sidebar");
  await extension.unload();
});
