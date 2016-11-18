/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testWindowCreate() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    background: async function() {
      const EXTENSION_URL = browser.runtime.getURL("test.html");
      const REMOTE_URL = browser.runtime.getURL("test.html");

      let windows = new class extends Map { // eslint-disable-line new-parens
        get(id) {
          if (!this.has(id)) {
            let window = {
              tabs: new Map(),
            };
            window.promise = new Promise(resolve => {
              window.resolvePromise = resolve;
            });

            this.set(id, window);
          }

          return super.get(id);
        }
      };

      browser.tabs.onUpdated.addListener((tabId, changed, tab) => {
        if (changed.status == "complete" && tab.url !== "about:blank") {
          let window = windows.get(tab.windowId);
          window.tabs.set(tab.index, tab);

          if (window.tabs.size === window.expectedTabs) {
            window.resolvePromise(window);
          }
        }
      });

      async function create(options) {
        let window = await browser.windows.create(options);
        let win = windows.get(window.id);
        win.id = window.id;

        win.expectedTabs = Array.isArray(options.url) ? options.url.length : 1;

        return win.promise;
      }

      try {
        let windows = await Promise.all([
          create({url: REMOTE_URL}),
          create({url: "test.html"}),
          create({url: EXTENSION_URL}),
          create({url: [REMOTE_URL, "test.html", EXTENSION_URL]}),
        ]);
        browser.test.assertEq(REMOTE_URL, windows[0].tabs.get(0).url, "Single, absolute, remote URL");

        browser.test.assertEq(REMOTE_URL, windows[1].tabs.get(0).url, "Single, relative URL");

        browser.test.assertEq(REMOTE_URL, windows[2].tabs.get(0).url, "Single, absolute, extension URL");

        browser.test.assertEq(REMOTE_URL, windows[3].tabs.get(0).url, "url[0]: Absolute, remote URL");
        browser.test.assertEq(EXTENSION_URL, windows[3].tabs.get(1).url, "url[1]: Relative URL");
        browser.test.assertEq(EXTENSION_URL, windows[3].tabs.get(2).url, "url[2]: Absolute, extension URL");

        Promise.all(windows.map(({id}) => browser.windows.remove(id))).then(() => {
          browser.test.notifyPass("window-create-url");
        });
      } catch (e) {
        browser.test.fail(`${e} :: ${e.stack}`);
        browser.test.notifyFail("window-create-url");
      }
    },

    files: {
      "test.html": `<DOCTYPE html><html><head><meta charset="utf-8"></head></html>`,
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("window-create-url");
  yield extension.unload();
});
