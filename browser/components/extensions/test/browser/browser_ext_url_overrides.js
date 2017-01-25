/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

const NEWTAB_URI_1 = "webext-newtab-1.html";
const NEWTAB_URI_2 = "webext-newtab-2.html";
const NEWTAB_URI_3 = "webext-newtab-3.html";

add_task(function* test_multiple_extensions_overriding_newtab_page() {
  is(aboutNewTabService.newTabURL, "about:newtab",
     "Default newtab url should be about:newtab");

  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_url_overrides": {}},
  });

  let ext2 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_url_overrides": {newtab: NEWTAB_URI_1}},
  });

  let ext3 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_url_overrides": {newtab: NEWTAB_URI_2}},
  });

  let ext4 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_url_overrides": {newtab: NEWTAB_URI_3}},
  });

  yield ext1.startup();

  is(aboutNewTabService.newTabURL, "about:newtab",
       "Default newtab url should still be about:newtab");

  yield ext2.startup();

  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_1),
     "Newtab url should be overriden by the second extension.");

  yield ext1.unload();

  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_1),
     "Newtab url should still be overriden by the second extension.");

  yield ext3.startup();

  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_1),
     "Newtab url should still be overriden by the second extension.");

  yield ext2.unload();

  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_2),
     "Newtab url should be overriden by the third extension.");

  yield ext4.startup();

  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_2),
     "Newtab url should be overriden by the third extension.");

  yield ext4.unload();

  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_2),
     "Newtab url should be overriden by the third extension.");

  yield ext3.unload();

  is(aboutNewTabService.newTabURL, "about:newtab",
     "Newtab url should be reset to about:newtab");
});

add_task(function* test_sending_message_from_newtab_page() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "chrome_url_overrides": {
        newtab: NEWTAB_URI_2,
      },
    },
    files: {
      [NEWTAB_URI_2]: `
        <!DOCTYPE html>
        <head>
          <meta charset="utf-8"/></head>
        <html>
          <body>
            <script src="newtab.js"></script>
          </body>
        </html>
      `,

      "newtab.js": function() {
        window.onload = () => {
          browser.test.sendMessage("from-newtab-page");
        };
      },
    },
  });

  yield extension.startup();

  // Simulate opening the newtab open as a user would.
  BrowserOpenTab();

  yield extension.awaitMessage("from-newtab-page");
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
  yield extension.unload();
});
