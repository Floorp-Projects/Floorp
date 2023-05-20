/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testIncognitoPopup() {
  let extension = ExtensionTestUtils.loadExtension({
    incognitoOverride: "spanning",
    manifest: {
      permissions: ["tabs"],
      browser_action: {
        default_popup: "popup.html",
        default_area: "navbar",
      },
      page_action: {
        default_popup: "popup.html",
      },
    },

    background: async function () {
      let resolveMessage;
      browser.runtime.onMessage.addListener(msg => {
        if (resolveMessage && msg.message == "popup-details") {
          resolveMessage(msg);
        }
      });

      const awaitPopup = windowId => {
        return new Promise(resolve => {
          resolveMessage = resolve;
        }).then(msg => {
          browser.test.assertEq(
            windowId,
            msg.windowId,
            "Got popup message from correct window"
          );
          return msg;
        });
      };

      const testWindow = async window => {
        const [tab] = await browser.tabs.query({
          active: true,
          windowId: window.id,
        });

        await browser.pageAction.show(tab.id);
        browser.test.sendMessage("click-pageAction");

        let msg = await awaitPopup(window.id);
        browser.test.assertEq(
          window.incognito,
          msg.incognito,
          "Correct incognito status in pageAction popup"
        );

        browser.test.sendMessage("click-browserAction");

        msg = await awaitPopup(window.id);
        browser.test.assertEq(
          window.incognito,
          msg.incognito,
          "Correct incognito status in browserAction popup"
        );
      };

      const testNonPrivateWindow = async () => {
        const window = await browser.windows.getCurrent();
        await testWindow(window);
      };

      const testPrivateWindow = async () => {
        const URL = "https://example.com/incognito";
        const windowReady = new Promise(resolve => {
          browser.tabs.onUpdated.addListener(function listener(
            tabId,
            changed,
            tab
          ) {
            if (changed.status == "complete" && tab.url == URL) {
              browser.tabs.onUpdated.removeListener(listener);
              resolve();
            }
          });
        });

        const window = await browser.windows.create({
          incognito: true,
          url: URL,
        });
        await windowReady;

        await testWindow(window);
      };

      browser.test.onMessage.addListener(async msg => {
        switch (msg) {
          case "test-nonprivate-window":
            await testNonPrivateWindow();
            break;
          case "test-private-window":
            await testPrivateWindow();
            break;
          default:
            browser.test.fail(
              `Unexpected test message: ${JSON.stringify(msg)}`
            );
        }

        browser.test.sendMessage(`${msg}:done`);
      });

      browser.test.sendMessage("bgscript:ready");
    },

    files: {
      "popup.html":
        '<html><head><meta charset="utf-8"><script src="popup.js"></script></head></html>',

      "popup.js": async function () {
        let win = await browser.windows.getCurrent();
        browser.runtime.sendMessage({
          message: "popup-details",
          windowId: win.id,
          incognito: browser.extension.inIncognitoContext,
        });
        window.close();
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("bgscript:ready");

  info("Run test on non private window");
  extension.sendMessage("test-nonprivate-window");
  await extension.awaitMessage("click-pageAction");
  const win = Services.wm.getMostRecentWindow("navigator:browser");
  ok(!PrivateBrowsingUtils.isWindowPrivate(win), "Got a nonprivate window");
  await clickPageAction(extension, win);

  await extension.awaitMessage("click-browserAction");
  await clickBrowserAction(extension, win);

  await extension.awaitMessage("test-nonprivate-window:done");
  await closeBrowserAction(extension, win);
  await closePageAction(extension, win);

  info("Run test on private window");
  extension.sendMessage("test-private-window");
  await extension.awaitMessage("click-pageAction");
  const privateWin = Services.wm.getMostRecentWindow("navigator:browser");
  ok(PrivateBrowsingUtils.isWindowPrivate(privateWin), "Got a private window");
  await clickPageAction(extension, privateWin);

  await extension.awaitMessage("click-browserAction");
  await clickBrowserAction(extension, privateWin);

  await extension.awaitMessage("test-private-window:done");
  // Wait for the private window chrome document to be flushed before
  // closing the browserACtion, pageAction and the entire private window,
  // to prevent intermittent failures.
  await privateWin.promiseDocumentFlushed(() => {});

  await closeBrowserAction(extension, privateWin);
  await closePageAction(extension, privateWin);
  await BrowserTestUtils.closeWindow(privateWin);

  await extension.unload();
});

add_task(async function test_pageAction_incognito_not_allowed() {
  const URL = "https://example.com/";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["*://example.com/*"],
      page_action: {
        show_matches: ["<all_urls>"],
        pinned: true,
      },
    },
  });

  await extension.startup();
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    URL,
    true,
    true
  );
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await BrowserTestUtils.openNewForegroundTab(
    privateWindow.gBrowser,
    URL,
    true,
    true
  );

  let elem = await getPageActionButton(extension, window);
  ok(elem, "pageAction button state correct in non-PB");

  elem = await getPageActionButton(extension, privateWindow);
  ok(!elem, "pageAction button state correct in private window");

  BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.closeWindow(privateWindow);
  await extension.unload();
});
