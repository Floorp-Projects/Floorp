/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* () {
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.net/");
  let window1 = yield BrowserTestUtils.openNewBrowserWindow();
  yield BrowserTestUtils.openNewForegroundTab(window1.gBrowser, "http://example.com/");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    async background() {
      let tabs = await browser.tabs.query({url: "<all_urls>"});
      let destination = tabs[0];
      let source = tabs[1]; // skip over about:blank in window1

      // Assuming that this windowId does not exist.
      await browser.tabs.move(source.id, {windowId: 123144576, index: 0}).then(
        tabs => {
          browser.test.fail("Promise should not resolve");
        },
        e => {
          browser.test.assertTrue(/Invalid window/.test(e),
                                  "Invalid window should be in error");
        });

      browser.tabs.move(source.id, {windowId: destination.windowId, index: 0});

      tabs = await browser.tabs.query({url: "<all_urls>"});
      browser.test.assertEq(tabs[0].url, "http://example.com/");
      browser.test.assertEq(tabs[0].windowId, destination.windowId);
      browser.test.notifyPass("tabs.move.window");
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.move.window");
  yield extension.unload();

  for (let tab of window.gBrowser.tabs) {
    yield BrowserTestUtils.removeTab(tab);
  }
  yield BrowserTestUtils.closeWindow(window1);
});

add_task(function* test_currentWindowAfterTabMoved() {
  const files = {
    "current.html": "<meta charset=utf-8><script src=current.js></script>",
    "current.js": function() {
      browser.test.onMessage.addListener(msg => {
        if (msg === "current") {
          browser.windows.getCurrent(win => {
            browser.test.sendMessage("id", win.id);
          });
        }
      });
      browser.test.sendMessage("ready");
    },
  };

  async function background() {
    let tabId;

    const url = browser.extension.getURL("current.html");

    browser.test.onMessage.addListener(async msg => {
      if (msg === "move") {
        await browser.windows.create({tabId});
        browser.test.sendMessage("moved");
      } else if (msg === "close") {
        await browser.tabs.remove(tabId);
        browser.test.sendMessage("done");
      }
    });

    let tab = await browser.tabs.create({url});
    tabId = tab.id;
  }

  const extension = ExtensionTestUtils.loadExtension({files, background});

  yield extension.startup();
  yield extension.awaitMessage("ready");

  extension.sendMessage("current");
  const first = yield extension.awaitMessage("id");

  extension.sendMessage("move");
  yield extension.awaitMessage("moved");

  extension.sendMessage("current");
  const second = yield extension.awaitMessage("id");

  isnot(first, second, "current window id is different after moving the tab");

  extension.sendMessage("close");
  yield extension.awaitMessage("done");
  yield extension.unload();
});
