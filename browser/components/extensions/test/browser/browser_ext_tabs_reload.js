/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    files: {
      "tab.js": function() {
        browser.runtime.sendMessage("tab-loaded");
      },
      "tab.html": `<head>
          <meta charset="utf-8">
          <script src="tab.js"></script>
        </head>`,
    },

    async background() {
      let tabLoadedCount = 0;

      let tab = await browser.tabs.create({ url: "tab.html", active: true });

      browser.runtime.onMessage.addListener(msg => {
        if (msg == "tab-loaded") {
          tabLoadedCount++;

          if (tabLoadedCount == 1) {
            // Reload the tab once passing no arguments.
            return browser.tabs.reload();
          }

          if (tabLoadedCount == 2) {
            // Reload the tab again with explicit arguments.
            return browser.tabs.reload(tab.id, {
              bypassCache: false,
            });
          }

          if (tabLoadedCount == 3) {
            browser.test.notifyPass("tabs.reload");
          }
        }
      });
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.reload");
  await extension.unload();
});
