/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* () {
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

  yield extension.startup();
  yield extension.awaitMessage("page-action-shown");

  clickPageAction(extension);

  yield extension.awaitMessage("popup");

  yield extension.unload();

  SimpleTest.endMonitorConsole();
  yield waitForConsole;
});
