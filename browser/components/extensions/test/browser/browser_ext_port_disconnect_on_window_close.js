/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Regression test for https://bugzil.la/1392067 .
add_task(async function connect_from_window_and_close() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.runtime.onConnect.addListener((port) => {
        browser.test.assertEq("page_to_bg", port.name, "expected port");
        port.onDisconnect.addListener(() => {
          browser.test.assertEq(null, port.error, "port should be disconnected without errors");
          browser.test.sendMessage("port_disconnected");
        });
        browser.windows.remove(port.sender.tab.windowId);
      });

      browser.windows.create({url: "page.html"});
    },

    files: {
      "page.html":
        `<!DOCTYPE html><meta charset="utf-8"><script src="page.js"></script>`,
      "page.js": function() {
        let port = browser.runtime.connect({name: "page_to_bg"});
        port.onDisconnect.addListener(() => {
          browser.test.fail("Unexpected onDisconnect event in page");
        });
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("port_disconnected");
  await extension.unload();
});
