/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testPageActionPopup() {
  const BASE =
    "http://example.com/browser/browser/components/extensions/test/browser";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: `${BASE}/file_popup_api_injection_a.html`,
      },
      page_action: {
        default_popup: `${BASE}/file_popup_api_injection_b.html`,
      },
    },

    files: {
      "popup-a.html": `<html><head><meta charset="utf-8">
                       <script type="application/javascript" src="popup-a.js"></script></head></html>`,
      "popup-a.js": 'browser.test.sendMessage("from-popup-a");',

      "popup-b.html": `<html><head><meta charset="utf-8">
                       <script type="application/javascript" src="popup-b.js"></script></head></html>`,
      "popup-b.js": 'browser.test.sendMessage("from-popup-b");',
    },

    background: function() {
      let tabId;
      browser.tabs.query({ active: true, currentWindow: true }, tabs => {
        tabId = tabs[0].id;
        browser.pageAction.show(tabId).then(() => {
          browser.test.sendMessage("ready");
        });
      });

      browser.test.onMessage.addListener(() => {
        browser.browserAction.setPopup({ popup: "/popup-a.html" });
        browser.pageAction.setPopup({ tabId, popup: "popup-b.html" });

        browser.test.sendMessage("ok");
      });
    },
  });

  let promiseConsoleMessage = pattern =>
    new Promise(resolve => {
      Services.console.registerListener(function listener(msg) {
        if (pattern.test(msg.message)) {
          resolve(msg.message);
          Services.console.unregisterListener(listener);
        }
      });
    });

  await extension.startup();
  await extension.awaitMessage("ready");

  // Check that unprivileged documents don't get the API.
  // BrowserAction:
  let awaitMessage = promiseConsoleMessage(
    /WebExt Privilege Escalation: BrowserAction/
  );
  SimpleTest.expectUncaughtException();
  await clickBrowserAction(extension);
  await awaitExtensionPanel(extension);

  let message = await awaitMessage;
  ok(
    message.includes(
      "WebExt Privilege Escalation: BrowserAction: typeof(browser) = undefined"
    ),
    `No BrowserAction API injection`
  );

  await closeBrowserAction(extension);

  // PageAction
  awaitMessage = promiseConsoleMessage(
    /WebExt Privilege Escalation: PageAction/
  );
  SimpleTest.expectUncaughtException();
  await clickPageAction(extension);

  message = await awaitMessage;
  ok(
    message.includes(
      "WebExt Privilege Escalation: PageAction: typeof(browser) = undefined"
    ),
    `No PageAction API injection: ${message}`
  );

  await closePageAction(extension);

  SimpleTest.expectUncaughtException(false);

  // Check that privileged documents *do* get the API.
  extension.sendMessage("next");
  await extension.awaitMessage("ok");

  await clickBrowserAction(extension);
  await awaitExtensionPanel(extension);
  await extension.awaitMessage("from-popup-a");
  await closeBrowserAction(extension);

  await clickPageAction(extension);
  await extension.awaitMessage("from-popup-b");
  await closePageAction(extension);

  await extension.unload();
});
