/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_popup_sendMessage_reply() {
  let scriptPage = url => `<html><head><meta charset="utf-8"><script src="${url}"></script></head><body>${url}</body></html>`;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {
        "default_popup": "popup.html",
        "browser_style": true,
      },

      "page_action": {
        "default_popup": "popup.html",
        "browser_style": true,
      },
    },

    files: {
      "popup.html": scriptPage("popup.js"),
      "popup.js": function() {
        browser.runtime.onMessage.addListener(msg => {
          if (msg == "popup-ping") {
            return Promise.resolve("popup-pong");
          }
        });

        browser.runtime.sendMessage("background-ping").then(response => {
          browser.test.sendMessage("background-ping-response", response);
        });
      },
    },

    background() {
      browser.tabs.query({active: true, currentWindow: true}).then(([tab]) => {
        return browser.pageAction.show(tab.id);
      }).then(() => {
        browser.test.sendMessage("page-action-ready");
      });

      browser.runtime.onMessage.addListener(msg => {
        if (msg == "background-ping") {
          browser.runtime.sendMessage("popup-ping").then(response => {
            browser.test.sendMessage("popup-ping-response", response);
          });

          return new Promise(resolve => {
            // Wait long enough that we're relatively sure the docShells have
            // been swapped. Note that this value is fairly arbitrary. The load
            // event that triggers the swap should happen almost immediately
            // after the message is sent. The extra quarter of a second gives us
            // enough leeway that we can expect to respond after the swap in the
            // vast majority of cases.
            setTimeout(resolve, 250);
          }).then(() => {
            return "background-pong";
          });
        }
      });
    },
  });

  yield extension.startup();

  {
    clickBrowserAction(extension);

    let pong = yield extension.awaitMessage("background-ping-response");
    is(pong, "background-pong", "Got pong");

    pong = yield extension.awaitMessage("popup-ping-response");
    is(pong, "popup-pong", "Got pong");

    yield closeBrowserAction(extension);
  }

  yield extension.awaitMessage("page-action-ready");

  {
    clickPageAction(extension);

    let pong = yield extension.awaitMessage("background-ping-response");
    is(pong, "background-pong", "Got pong");

    pong = yield extension.awaitMessage("popup-ping-response");
    is(pong, "popup-pong", "Got pong");

    yield closePageAction(extension);
  }

  yield extension.unload();
});
