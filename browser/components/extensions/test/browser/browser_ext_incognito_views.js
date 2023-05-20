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
      description: JSON.stringify({
        headless: Services.env.get("MOZ_HEADLESS"),
        debug: AppConstants.DEBUG,
      }),
      browser_action: {
        default_popup: "popup.html",
        default_area: "navbar",
      },
    },

    background: async function () {
      window.isBackgroundPage = true;
      const { headless, debug } = JSON.parse(
        browser.runtime.getManifest().description
      );

      class ConnectedPopup {
        #msgPromise;
        #disconnectPromise;

        static promiseNewPopup() {
          return new Promise(resolvePort => {
            browser.runtime.onConnect.addListener(function onConnect(port) {
              browser.runtime.onConnect.removeListener(onConnect);
              browser.test.assertEq("from-popup", port.name, "Port from popup");
              resolvePort(new ConnectedPopup(port));
            });
          });
        }

        constructor(port) {
          this.port = port;
          this.#msgPromise = new Promise(resolveMessage => {
            // popup.js sends one message with the popup's metadata.
            port.onMessage.addListener(resolveMessage);
          });
          this.#disconnectPromise = new Promise(resolveDisconnect => {
            port.onDisconnect.addListener(resolveDisconnect);
          });
        }

        async getPromisedMessage() {
          browser.test.log("Waiting for popup to send information");
          let msg = await this.#msgPromise;
          browser.test.assertEq("popup-details", msg.message, "Got port msg");
          return msg;
        }

        async promisePopupClosed(desc) {
          browser.test.log(`Waiting for popup to be closed (${desc})`);
          // There is currently no great way for extension to detect a closed
          // popup. Extensions can observe the port.onDisconnect event for now.
          await this.#disconnectPromise;
          browser.test.log(`Popup was closed (${desc})`);
        }

        async closePopup(desc) {
          browser.test.log(`Closing popup (${desc})`);
          this.port.postMessage("close_popup");
          return this.promisePopupClosed(desc);
        }
      }

      let testPopupForWindow = async window => {
        let popupConnectionPromise = ConnectedPopup.promiseNewPopup();

        await browser.browserAction.openPopup({
          windowId: window.id,
        });

        let connectedPopup = await popupConnectionPromise;
        let msg = await connectedPopup.getPromisedMessage();
        browser.test.assertEq(
          window.id,
          msg.windowId,
          "Got popup message from correct window"
        );
        browser.test.assertEq(
          window.incognito,
          msg.incognito,
          "Correct incognito status in browserAction popup"
        );

        return connectedPopup;
      };

      async function createPrivateWindow() {
        const URL = "https://example.com/?dummy-incognito-window";
        let windowReady = new Promise(resolve => {
          browser.tabs.onUpdated.addListener(function l(tabId, changed, tab) {
            if (changed.status == "complete" && tab.url == URL) {
              browser.tabs.onUpdated.removeListener(l);
              resolve();
            }
          });
        });
        let window = await browser.windows.create({
          incognito: true,
          url: URL,
        });
        await windowReady;
        return window;
      }

      function getNonPrivateViewCount() {
        // The background context is in non-private browsing mode, so getViews()
        // only returns the views that are not in private browsing mode.
        return browser.extension.getViews({ type: "popup" }).length;
      }

      try {
        let nonPrivatePopup;
        {
          let window = await browser.windows.getCurrent();

          nonPrivatePopup = await testPopupForWindow(window);

          browser.test.assertEq(1, getNonPrivateViewCount(), "popup is open");
          // ^ The popup will close when a new window is opened below.
          if (headless) {
            // ... except when --headless is used. For some reason, the popup
            // does not close when another window is opened. Close manually.
            await nonPrivatePopup.closePopup("Work-around for --headless bug");
          }
        }

        {
          let window = await createPrivateWindow();

          let privatePopup = await testPopupForWindow(window);

          await nonPrivatePopup.promisePopupClosed("First popup closed by now");

          browser.test.assertEq(
            0,
            getNonPrivateViewCount(),
            "First popup should have been closed when a new window was opened"
          );

          // TODO bug 1809000: On debug builds, a memory leak is reported when
          // the popup is closed as part of closing a window. As a work-around,
          // we explicitly close the popup here.
          // TODO: Remove when bug 1809000 is fixed.
          if (debug) {
            await privatePopup.closePopup("Work-around for bug 1809000");
          }

          await browser.windows.remove(window.id);
          // ^ This also closes the popup panel associated with the window. If
          // it somehow does not close properly, errors may be reported, e.g.
          // leakcheck failures in debug mode (like bug 1800100).

          // This check is not strictly necessary, but we're doing this to
          // confirm that the private popup has indeed been closed.
          await privatePopup.promisePopupClosed("Window closed = popup gone");
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

      "popup.js": async function () {
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
        let port = browser.runtime.connect({ name: "from-popup" });
        port.onMessage.addListener(msg => {
          browser.test.assertEq("close_popup", msg, "Close popup msg");
          window.close();
        });
        port.postMessage({
          message: "popup-details",
          windowId: win.id,
          incognito: browser.extension.inIncognitoContext,
        });
      },
    },
  });

  await extension.startup();
  await extension.awaitFinish("incognito-views");
  await extension.unload();
});
