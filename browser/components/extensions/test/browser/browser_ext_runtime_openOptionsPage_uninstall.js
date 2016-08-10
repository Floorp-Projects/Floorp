/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function* loadExtension(options) {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",

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
        browser.runtime.sendMessage("options.html");
        browser.runtime.onMessage.addListener((msg, sender, respond) => {
          if (msg == "ping") {
            respond("pong");
          }
        });
      },
    },

    background: options.background,
  });

  yield extension.startup();

  return extension;
}

add_task(function* test_inline_options_uninstall() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");

  let extension = yield loadExtension({
    manifest: {
      applications: {gecko: {id: "inline_options_uninstall@tests.mozilla.org"}},
      "options_ui": {
        "page": "options.html",
      },
    },

    background: function() {
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

      let firstTab;
      browser.tabs.query({currentWindow: true, active: true}).then(tabs => {
        firstTab = tabs[0].id;

        browser.test.log("Open options page. Expect fresh load.");
        return Promise.all([
          browser.runtime.openOptionsPage(),
          awaitOptions(),
        ]);
      }).then(([, tab]) => {
        browser.test.assertEq("about:addons", tab.url, "Tab contains AddonManager");
        browser.test.assertTrue(tab.active, "Tab is active");
        browser.test.assertTrue(tab.id != firstTab, "Tab is a new tab");

        browser.test.sendMessage("options-ui-open");
      }).catch(error => {
        browser.test.fail(`Error: ${error} :: ${error.stack}`);
      });
    },
  });

  yield extension.awaitMessage("options-ui-open");
  yield extension.unload();

  is(gBrowser.selectedBrowser.currentURI.spec, "about:addons",
     "Add-on manager tab should still be open");

  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);

  yield BrowserTestUtils.removeTab(tab);
});
