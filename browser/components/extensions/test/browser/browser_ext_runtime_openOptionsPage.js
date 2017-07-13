/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

requestLongerTimeout(2);

function add_tasks(task) {
  add_task(task.bind(null, {embedded: false}));

  add_task(task.bind(null, {embedded: true}));
}

async function loadExtension(options) {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",

    embedded: options.embedded,

    manifest: Object.assign({
      "permissions": ["tabs"],
    }, options.manifest),

    files: {
      "options.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="options.js" type="text/javascript"></script>
          </head>
        </html>`,

      "options.js": function() {
        window.iAmOption = true;
        browser.runtime.sendMessage("options.html");
        browser.runtime.onMessage.addListener((msg, sender, respond) => {
          if (msg == "ping") {
            respond("pong");
          } else if (msg == "connect") {
            let port = browser.runtime.connect();
            port.postMessage("ping-from-options-html");
            port.onMessage.addListener(msg => {
              if (msg == "ping-from-bg") {
                browser.test.log("Got outbound options.html pong");
                browser.test.sendMessage("options-html-outbound-pong");
              }
            });
          }
        });

        browser.runtime.onConnect.addListener(port => {
          browser.test.log("Got inbound options.html port");

          port.postMessage("ping-from-options-html");
          port.onMessage.addListener(msg => {
            if (msg == "ping-from-bg") {
              browser.test.log("Got inbound options.html pong");
              browser.test.sendMessage("options-html-inbound-pong");
            }
          });
        });
      },
    },

    background: options.background,
  });

  await extension.startup();

  return extension;
}

add_tasks(async function test_inline_options(extraOptions) {
  info(`Test options opened inline (${JSON.stringify(extraOptions)})`);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");

  let extension = await loadExtension(Object.assign({}, extraOptions, {
    manifest: {
      applications: {gecko: {id: "inline_options@tests.mozilla.org"}},
      "options_ui": {
        "page": "options.html",
      },
    },

    background: async function() {
      let _optionsPromise;
      let awaitOptions = () => {
        browser.test.assertFalse(_optionsPromise, "Should not be awaiting options already");

        return new Promise(resolve => {
          _optionsPromise = {resolve};
        });
      };

      browser.runtime.onMessage.addListener((msg, sender) => {
        if (msg == "options.html") {
          if (_optionsPromise) {
            _optionsPromise.resolve(sender.tab);
            _optionsPromise = null;
          } else {
            browser.test.fail("Saw unexpected options page load");
          }
        }
      });

      try {
        let [firstTab] = await browser.tabs.query({currentWindow: true, active: true});

        browser.test.log("Open options page. Expect fresh load.");

        let [, optionsTab] = await Promise.all([
          browser.runtime.openOptionsPage(),
          awaitOptions(),
        ]);

        browser.test.assertEq("about:addons", optionsTab.url, "Tab contains AddonManager");
        browser.test.assertTrue(optionsTab.active, "Tab is active");
        browser.test.assertTrue(optionsTab.id != firstTab.id, "Tab is a new tab");

        browser.test.assertEq(0, browser.extension.getViews({type: "popup"}).length, "viewType is not popup");
        browser.test.assertEq(1, browser.extension.getViews({type: "tab"}).length, "viewType is tab");
        browser.test.assertEq(1, browser.extension.getViews({windowId: optionsTab.windowId}).length, "windowId matches");

        let views = browser.extension.getViews();
        browser.test.assertEq(2, views.length, "Expected the options page and the background page");
        browser.test.assertTrue(views.includes(window), "One of the views is the background page");
        browser.test.assertTrue(views.some(w => w.iAmOption), "One of the views is the options page");

        browser.test.log("Switch tabs.");
        await browser.tabs.update(firstTab.id, {active: true});

        browser.test.log("Open options page again. Expect tab re-selected, no new load.");

        await browser.runtime.openOptionsPage();
        let [tab] = await browser.tabs.query({currentWindow: true, active: true});

        browser.test.assertEq(optionsTab.id, tab.id, "Tab is the same as the previous options tab");
        browser.test.assertEq("about:addons", tab.url, "Tab contains AddonManager");

        browser.test.log("Ping options page.");
        let pong = await browser.runtime.sendMessage("ping");
        browser.test.assertEq("pong", pong, "Got pong.");

        let done = new Promise(resolve => {
          browser.test.onMessage.addListener(msg => {
            if (msg == "ports-done") {
              resolve();
            }
          });
        });

        browser.runtime.onConnect.addListener(port => {
          browser.test.log("Got inbound background port");

          port.postMessage("ping-from-bg");
          port.onMessage.addListener(msg => {
            if (msg == "ping-from-options-html") {
              browser.test.log("Got inbound background pong");
              browser.test.sendMessage("bg-inbound-pong");
            }
          });
        });

        browser.runtime.sendMessage("connect");

        let port = browser.runtime.connect();
        port.postMessage("ping-from-bg");
        port.onMessage.addListener(msg => {
          if (msg == "ping-from-options-html") {
            browser.test.log("Got outbound background pong");
            browser.test.sendMessage("bg-outbound-pong");
          }
        });

        await done;

        browser.test.log("Remove options tab.");
        await browser.tabs.remove(optionsTab.id);

        browser.test.log("Open options page again. Expect fresh load.");
        [, tab] = await Promise.all([
          browser.runtime.openOptionsPage(),
          awaitOptions(),
        ]);
        browser.test.assertEq("about:addons", tab.url, "Tab contains AddonManager");
        browser.test.assertTrue(tab.active, "Tab is active");
        browser.test.assertTrue(tab.id != optionsTab.id, "Tab is a new tab");

        await browser.tabs.remove(tab.id);

        browser.test.notifyPass("options-ui");
      } catch (error) {
        browser.test.fail(`Error: ${error} :: ${error.stack}`);
        browser.test.notifyFail("options-ui");
      }
    },
  }));

  await Promise.all([
    extension.awaitMessage("options-html-inbound-pong"),
    extension.awaitMessage("options-html-outbound-pong"),
    extension.awaitMessage("bg-inbound-pong"),
    extension.awaitMessage("bg-outbound-pong"),
  ]);

  extension.sendMessage("ports-done");

  await extension.awaitFinish("options-ui");

  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});

add_tasks(async function test_tab_options(extraOptions) {
  info(`Test options opened in a tab (${JSON.stringify(extraOptions)})`);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");

  let extension = await loadExtension(Object.assign({}, extraOptions, {
    manifest: {
      applications: {gecko: {id: "tab_options@tests.mozilla.org"}},
      "options_ui": {
        "page": "options.html",
        "open_in_tab": true,
      },
    },

    background: async function() {
      let _optionsPromise;
      let awaitOptions = () => {
        browser.test.assertFalse(_optionsPromise, "Should not be awaiting options already");

        return new Promise(resolve => {
          _optionsPromise = {resolve};
        });
      };

      browser.runtime.onMessage.addListener((msg, sender) => {
        if (msg == "options.html") {
          if (_optionsPromise) {
            _optionsPromise.resolve(sender.tab);
            _optionsPromise = null;
          } else {
            browser.test.fail("Saw unexpected options page load");
          }
        }
      });

      let optionsURL = browser.extension.getURL("options.html");

      try {
        let [firstTab] = await browser.tabs.query({currentWindow: true, active: true});

        browser.test.log("Open options page. Expect fresh load.");
        let [, optionsTab] = await Promise.all([
          browser.runtime.openOptionsPage(),
          awaitOptions(),
        ]);
        browser.test.assertEq(optionsURL, optionsTab.url, "Tab contains options.html");
        browser.test.assertTrue(optionsTab.active, "Tab is active");
        browser.test.assertTrue(optionsTab.id != firstTab.id, "Tab is a new tab");

        browser.test.assertEq(0, browser.extension.getViews({type: "popup"}).length, "viewType is not popup");
        browser.test.assertEq(1, browser.extension.getViews({type: "tab"}).length, "viewType is tab");
        browser.test.assertEq(1, browser.extension.getViews({windowId: optionsTab.windowId}).length, "windowId matches");

        let views = browser.extension.getViews();
        browser.test.assertEq(2, views.length, "Expected the options page and the background page");
        browser.test.assertTrue(views.includes(window), "One of the views is the background page");
        browser.test.assertTrue(views.some(w => w.iAmOption), "One of the views is the options page");

        browser.test.log("Switch tabs.");
        await browser.tabs.update(firstTab.id, {active: true});

        browser.test.log("Open options page again. Expect tab re-selected, no new load.");

        await browser.runtime.openOptionsPage();
        let [tab] = await browser.tabs.query({currentWindow: true, active: true});

        browser.test.assertEq(optionsTab.id, tab.id, "Tab is the same as the previous options tab");
        browser.test.assertEq(optionsURL, tab.url, "Tab contains options.html");

        // Unfortunately, we can't currently do this, since onMessage doesn't
        // currently support responses when there are multiple listeners.
        //
        // browser.test.log("Ping options page.");
        // return new Promise(resolve => browser.runtime.sendMessage("ping", resolve));

        browser.test.log("Remove options tab.");
        await browser.tabs.remove(optionsTab.id);

        browser.test.log("Open options page again. Expect fresh load.");
        [, tab] = await Promise.all([
          browser.runtime.openOptionsPage(),
          awaitOptions(),
        ]);
        browser.test.assertEq(optionsURL, tab.url, "Tab contains options.html");
        browser.test.assertTrue(tab.active, "Tab is active");
        browser.test.assertTrue(tab.id != optionsTab.id, "Tab is a new tab");

        await browser.tabs.remove(tab.id);

        browser.test.notifyPass("options-ui-tab");
      } catch (error) {
        browser.test.fail(`Error: ${error} :: ${error.stack}`);
        browser.test.notifyFail("options-ui-tab");
      }
    },
  }));

  await extension.awaitFinish("options-ui-tab");
  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});

add_tasks(async function test_options_no_manifest(extraOptions) {
  info(`Test with no manifest key (${JSON.stringify(extraOptions)})`);

  let extension = await loadExtension(Object.assign({}, extraOptions, {
    manifest: {
      applications: {gecko: {id: "no_options@tests.mozilla.org"}},
    },

    async background() {
      browser.test.log("Try to open options page when not specified in the manifest.");

      await browser.test.assertRejects(
        browser.runtime.openOptionsPage(),
        /No `options_ui` declared/,
        "Expected error from openOptionsPage()");

      browser.test.notifyPass("options-no-manifest");
    },
  }));

  await extension.awaitFinish("options-no-manifest");
  await extension.unload();
});
