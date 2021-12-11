/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function test_popup_sendMessage_reply() {
  let scriptPage = url =>
    `<html><head><meta charset="utf-8"><script src="${url}"></script></head><body>${url}</body></html>`;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: "popup.html",
        browser_style: true,
      },

      page_action: {
        default_popup: "popup.html",
        browser_style: true,
      },
    },

    files: {
      "popup.html": scriptPage("popup.js"),
      "popup.js": async function() {
        browser.runtime.onMessage.addListener(async msg => {
          if (msg == "popup-ping") {
            return "popup-pong";
          }
        });

        let response = await browser.runtime.sendMessage("background-ping");
        browser.test.sendMessage("background-ping-response", response);
      },
    },

    async background() {
      browser.runtime.onMessage.addListener(async msg => {
        if (msg == "background-ping") {
          let response = await browser.runtime.sendMessage("popup-ping");

          browser.test.sendMessage("popup-ping-response", response);

          await new Promise(resolve => {
            // Wait long enough that we're relatively sure the docShells have
            // been swapped. Note that this value is fairly arbitrary. The load
            // event that triggers the swap should happen almost immediately
            // after the message is sent. The extra quarter of a second gives us
            // enough leeway that we can expect to respond after the swap in the
            // vast majority of cases.
            setTimeout(resolve, 250);
          });

          return "background-pong";
        }
      });

      let [tab] = await browser.tabs.query({
        active: true,
        currentWindow: true,
      });

      await browser.pageAction.show(tab.id);

      browser.test.sendMessage("page-action-ready");
    },
  });

  await extension.startup();

  {
    clickBrowserAction(extension);

    let pong = await extension.awaitMessage("background-ping-response");
    is(pong, "background-pong", "Got pong");

    pong = await extension.awaitMessage("popup-ping-response");
    is(pong, "popup-pong", "Got pong");

    await closeBrowserAction(extension);
  }

  await extension.awaitMessage("page-action-ready");

  {
    clickPageAction(extension);

    let pong = await extension.awaitMessage("background-ping-response");
    is(pong, "background-pong", "Got pong");

    pong = await extension.awaitMessage("popup-ping-response");
    is(pong, "popup-pong", "Got pong");

    await closePageAction(extension);
  }

  await extension.unload();
});

add_task(async function test_popup_close_then_sendMessage() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: "popup.html",
      },
    },

    files: {
      "popup.html": `<meta charset="utf-8"><script src="popup.js" defer></script>ghost`,
      "popup.js"() {
        browser.tabs.query({ active: true }).then(() => {
          // NOTE: the message will be sent _after_ the popup is closed below.
          browser.runtime.sendMessage("sent-after-closed");
        });
        window.close();
      },
    },

    async background() {
      browser.runtime.onMessage.addListener(msg => {
        browser.test.assertEq(msg, "sent-after-closed", "Message from popup.");
        browser.test.sendMessage("done");
      });
      browser.test.sendMessage("ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  clickBrowserAction(extension);
  await extension.awaitMessage("done");

  await extension.unload();
});
