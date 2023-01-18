/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.openPopupWithoutUserGesture.enabled", true]],
  });
});

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
        default_area: "navbar",
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
      let promisePopupDetails = () => {
        return new Promise(resolve => {
          resolveMessage = resolve;
        });
      };

      let testWindow = async window => {
        let popupDetailsPromise = promisePopupDetails();

        await browser.browserAction.openPopup({
          windowId: window.id,
        });

        let msg = await popupDetailsPromise;
        browser.test.assertEq(
          window.id,
          msg.windowId,
          "Got popup message from correct window"
        );
        browser.test.assertDeepEq(
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

      function getNonPrivateViewCount() {
        // The background context is in non-private browsing mode, so getViews()
        // only returns the views that are not in private browsing mode.
        return browser.extension.getViews({ type: "popup" }).length;
      }

      try {
        {
          let window = await browser.windows.getCurrent();

          await testWindow(window);

          browser.test.assertEq(1, getNonPrivateViewCount(), "popup is open");
          // ^ The popup will close when a new window is opened below.
        }

        {
          let window = await browser.windows.create({
            incognito: true,
            url: URL,
          });
          await windowReady;

          await testWindow(window);

          browser.test.assertEq(
            0,
            getNonPrivateViewCount(),
            "First popup should have been closed when a new window was opened"
          );

          await browser.windows.remove(window.id);
          // ^ This also closes the popup panel associated with the window. If
          // it somehow does not close properly, errors may be reported, e.g.
          // leakcheck failures in debug mode (like bug 1800100).
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

        // TODO bug 1809000: On debug builds, a memory leak is reported when
        // the popup is closed as part of closing a window. As a work-around,
        // we explicitly close the popup here.
        // TODO: Remove when bug 1809000 is fixed.
        if (browser.extension.inIncognitoContext) {
          window.close();
        }
      },
    },
  });

  await extension.startup();
  await extension.awaitFinish("incognito-views");
  await extension.unload();
});
