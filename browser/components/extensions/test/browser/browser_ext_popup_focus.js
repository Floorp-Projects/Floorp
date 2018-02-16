/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const DUMMY_PAGE = "http://example.com/browser/browser/components/extensions/test/browser/file_dummy.html";

add_task(async function testPageActionFocus() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "page_action": {
        "default_popup": "popup.html",
        "show_matches": ["<all_urls>"],
      },
    },
    files: {
      "popup.html": `<!DOCTYPE html><html><head><meta charset="utf-8">
        <script src="popup.js"></script>
        </head><body>
        </body></html>
      `,
      "popup.js": function() {
        window.addEventListener("focus", (event) => {
          browser.test.assertEq(true, document.hasFocus(), "document should be focused");
          browser.test.notifyPass("focused");
        }, {once: true});
      },
    },
  });
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, DUMMY_PAGE);

  await extension.startup();
  let finish = extension.awaitFinish("focused");
  await clickPageAction(extension);
  await finish;
  await closePageAction(extension);

  await BrowserTestUtils.removeTab(tab);
  await extension.unload();
});

add_task(async function testBrowserActionFocus() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {"default_popup": "popup.html"},
    },
    files: {
      "popup.html": `<!DOCTYPE html><html><head><meta charset="utf-8">
        <script src="popup.js"></script>
        </head><body>
        </body></html>
      `,
      "popup.js": function() {
        window.addEventListener("focus", (event) => {
          browser.test.assertEq(true, document.hasFocus(), "document should be focused");
          browser.test.notifyPass("focused");
        }, {once: true});
      },
    },
  });
  await extension.startup();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, DUMMY_PAGE);
  let finish = extension.awaitFinish("focused");
  await clickBrowserAction(extension);
  await finish;

  await closeBrowserAction(extension);

  await BrowserTestUtils.removeTab(tab);
  await extension.unload();
});
