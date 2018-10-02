/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.import("resource:///modules/PageActions.jsm");

add_task(async function test_pageAction_basic() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "page_action": {
        "default_popup": "popup.html",
        "unrecognized_property": "with-a-random-value",
      },
    },

    files: {
      "popup.html": `
      <!DOCTYPE html>
      <html><body>
      <script src="popup.js"></script>
      </body></html>
      `,

      "popup.js": function() {
        browser.runtime.sendMessage("from-popup");
      },
    },

    background: function() {
      browser.runtime.onMessage.addListener(msg => {
        browser.test.assertEq(msg, "from-popup", "correct message received");
        browser.test.sendMessage("popup");
      });
      browser.tabs.query({active: true, currentWindow: true}, tabs => {
        let tabId = tabs[0].id;

        browser.pageAction.show(tabId).then(() => {
          browser.test.sendMessage("page-action-shown");
        });
      });
    },
  });

  SimpleTest.waitForExplicitFinish();
  let waitForConsole = new Promise(resolve => {
    SimpleTest.monitorConsole(resolve, [{
      message: /Reading manifest: Error processing page_action.unrecognized_property: An unexpected property was found/,
    }]);
  });

  await extension.startup();
  await extension.awaitMessage("page-action-shown");

  let elem = await getPageActionButton(extension);
  let parent = window.document.getElementById("page-action-buttons");
  is(elem && elem.parentNode, parent, `pageAction pinned to urlbar ${elem.parentNode.getAttribute("id")}`);

  clickPageAction(extension);

  await extension.awaitMessage("popup");

  await extension.unload();

  SimpleTest.endMonitorConsole();
  await waitForConsole;
});

add_task(async function test_pageAction_pinned() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "page_action": {
        "default_popup": "popup.html",
        "pinned": false,
      },
    },

    files: {
      "popup.html": `
      <!DOCTYPE html>
      <html><body>
      </body></html>
      `,
    },

    background: function() {
      browser.tabs.query({active: true, currentWindow: true}, tabs => {
        let tabId = tabs[0].id;

        browser.pageAction.show(tabId).then(() => {
          browser.test.sendMessage("page-action-shown");
        });
      });
    },
  });

  await extension.startup();
  await extension.awaitMessage("page-action-shown");

  let elem = await getPageActionButton(extension);
  is(elem && elem.parentNode, null, "pageAction is not pinned to urlbar");

  // There are plenty of tests for the main action button, we just verify
  // that we've properly set the pinned value to false.
  let action = PageActions.actionForID(makeWidgetId(extension.id));
  ok(action && !action.pinnedToUrlbar, "pageAction is in main pageaction menu");

  await extension.unload();
});
