/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

let scriptPage = url =>
  `<html><head><meta charset="utf-8"><script src="${url}"></script></head><body>${url}</body></html>`;

// Tests that message ports still function correctly after a browserAction popup
// <browser> has been reparented.
add_task(async function test_browserActionPort() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: "popup.html",
        browser_style: true,
      },
    },

    background() {
      new Promise(resolve => {
        browser.runtime.onConnect.addListener(port => {
          resolve(
            Promise.all([
              new Promise(r => port.onMessage.addListener(r)),
              new Promise(r => port.onDisconnect.addListener(r)),
            ])
          );
        });
      }).then(([msg]) => {
        browser.test.assertEq("Hallo.", msg, "Got expected message");
        browser.test.notifyPass("browserAction-popup-port");
      });
    },

    files: {
      "popup.html": scriptPage("popup.js"),
      "popup.js"() {
        let port = browser.runtime.connect();
        window.onload = () => {
          setTimeout(() => {
            port.postMessage("Hallo.");
            window.close();
          }, 0);
        };
      },
    },
  });

  await extension.startup();

  await clickBrowserAction(extension);

  await extension.awaitFinish("browserAction-popup-port");
  await extension.unload();
});
