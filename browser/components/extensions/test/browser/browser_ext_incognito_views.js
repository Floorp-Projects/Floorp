/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testIncognitoViews() {
  // Make sure the mouse isn't hovering over the browserAction widget.
  EventUtils.synthesizeMouseAtCenter(
    gURLBar.textbox,
    { type: "mouseover" },
    window
  );

  let extension = ExtensionTestUtils.loadExtension({
    incognitoOverride: "spanning",
    manifest: {
      permissions: ["tabs"],
      browser_action: {
        default_popup: "popup.html",
      },
    },

    background: async function() {
      window.isBackgroundPage = true;

      let resolveMessage;
      browser.runtime.onMessage.addListener(msg => {
        if (resolveMessage && msg.message == "popup-details") {
          resolveMessage(msg);
        }
      });

      let awaitPopup = windowId => {
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

      let testWindow = async window => {
        browser.test.sendMessage("click-browserAction");

        let msg = await awaitPopup(window.id);
        browser.test.assertEq(
          window.incognito,
          msg.incognito,
          "Correct incognito status in browserAction popup"
        );
      };

      const URL = "https://example.com/incognito";
      let windowReady = new Promise(resolve => {
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

      try {
        {
          let window = await browser.windows.getCurrent();

          await testWindow(window);
        }

        {
          let window = await browser.windows.create({
            incognito: true,
            url: URL,
          });
          await windowReady;

          await testWindow(window);
        }

        browser.test.notifyPass("incognito-views");
      } catch (error) {
        browser.test.fail(`Error: ${error} :: ${error.stack}`);
        browser.test.notifyFail("incognito-views");
      }
    },

    files: {
      "popup.html":
        '<html><head><meta charset="utf-8"><script src="popup.js"></script></head></html>',

      "popup.js": async function() {
        let views = browser.extension.getViews();

        if (browser.extension.inIncognitoContext) {
          let bgPage = browser.extension.getBackgroundPage();
          browser.test.assertEq(
            null,
            bgPage,
            "Should not be able to access background page in incognito context"
          );

          bgPage = await browser.runtime.getBackgroundPage();
          browser.test.assertEq(
            null,
            bgPage,
            "Should not be able to access background page in incognito context"
          );

          browser.test.assertEq(
            1,
            views.length,
            "Should only see one view in incognito popup"
          );
          browser.test.assertEq(
            window,
            views[0],
            "This window should be the only view"
          );
        } else {
          let bgPage = browser.extension.getBackgroundPage();
          browser.test.assertEq(
            true,
            bgPage.isBackgroundPage,
            "Should be able to access background page in non-incognito context"
          );

          bgPage = await browser.runtime.getBackgroundPage();
          browser.test.assertEq(
            true,
            bgPage.isBackgroundPage,
            "Should be able to access background page in non-incognito context"
          );

          browser.test.assertEq(
            2,
            views.length,
            "Should only two views in non-incognito popup"
          );
          browser.test.assertEq(
            bgPage,
            views[0],
            "The background page should be the first view"
          );
          browser.test.assertEq(
            window,
            views[1],
            "This window should be the second view"
          );
        }

        let win = await browser.windows.getCurrent();
        browser.runtime.sendMessage({
          message: "popup-details",
          windowId: win.id,
          incognito: browser.extension.inIncognitoContext,
        });
      },
    },
  });

  let win;
  let promiseBrowserActionOpened;
  extension.onMessage("click-browserAction", () => {
    win = Services.wm.getMostRecentWindow("navigator:browser");
    promiseBrowserActionOpened = openBrowserActionPanel(extension, win, true);
  });

  await extension.startup();
  await extension.awaitFinish("incognito-views");
  // Prevent intermittent failures of this test in optimized builds due to a race between
  // opening/closing the browserAction and closing the related window at the end
  // of the test (e.g. Bug 1707305).
  await promiseBrowserActionOpened;
  await closeBrowserAction(extension, win);
  await extension.unload();
  await BrowserTestUtils.closeWindow(win);
});
