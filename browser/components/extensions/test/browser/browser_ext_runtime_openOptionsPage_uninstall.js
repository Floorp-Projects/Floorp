/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function loadExtension(options) {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",

    manifest: Object.assign(
      {
        permissions: ["tabs"],
      },
      options.manifest
    ),

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

  await extension.startup();

  return extension;
}

add_task(async function test_inline_options_uninstall() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/"
  );

  let extension = await loadExtension({
    manifest: {
      applications: {
        gecko: { id: "inline_options_uninstall@tests.mozilla.org" },
      },
      options_ui: {
        page: "options.html",
      },
    },

    background: async function() {
      let _optionsPromise;
      let awaitOptions = () => {
        browser.test.assertFalse(
          _optionsPromise,
          "Should not be awaiting options already"
        );

        return new Promise(resolve => {
          _optionsPromise = { resolve };
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
        let [firstTab] = await browser.tabs.query({
          currentWindow: true,
          active: true,
        });

        browser.test.log("Open options page. Expect fresh load.");
        let [, tab] = await Promise.all([
          browser.runtime.openOptionsPage(),
          awaitOptions(),
        ]);

        browser.test.assertEq(
          "about:addons",
          tab.url,
          "Tab contains AddonManager"
        );
        browser.test.assertTrue(tab.active, "Tab is active");
        browser.test.assertTrue(tab.id != firstTab.id, "Tab is a new tab");

        browser.test.sendMessage("options-ui-open");
      } catch (error) {
        browser.test.fail(`Error: ${error} :: ${error.stack}`);
      }
    },
  });

  await extension.awaitMessage("options-ui-open");
  await extension.unload();

  is(
    gBrowser.selectedBrowser.currentURI.spec,
    "about:addons",
    "Add-on manager tab should still be open"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  BrowserTestUtils.removeTab(tab);
});
