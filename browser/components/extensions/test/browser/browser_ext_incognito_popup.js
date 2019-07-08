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
      },
      page_action: {
        default_popup: "popup.html",
      },
    },

    background: async function() {
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
        let [tab] = await browser.tabs.query({
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

      const URL = "http://example.com/incognito";
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

          await browser.windows.remove(window.id);
        }

        browser.test.notifyPass("incognito");
      } catch (error) {
        browser.test.fail(`Error: ${error} :: ${error.stack}`);
        browser.test.notifyFail("incognito");
      }
    },

    files: {
      "popup.html":
        '<html><head><meta charset="utf-8"><script src="popup.js"></script></head></html>',

      "popup.js": async function() {
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

  extension.onMessage("click-browserAction", () => {
    clickBrowserAction(
      extension,
      Services.wm.getMostRecentWindow("navigator:browser")
    );
  });

  extension.onMessage("click-pageAction", () => {
    clickPageAction(
      extension,
      Services.wm.getMostRecentWindow("navigator:browser")
    );
  });

  await extension.startup();
  await extension.awaitFinish("incognito");
  await extension.unload();
});

add_task(async function test_pageAction_incognito_not_allowed() {
  SpecialPowers.pushPrefEnv({
    set: [["extensions.allowPrivateBrowsingByDefault", false]],
  });

  const URL = "http://example.com/";
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
