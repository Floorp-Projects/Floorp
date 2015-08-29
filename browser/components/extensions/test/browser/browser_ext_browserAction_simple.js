add_task(function* () {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {
        "default_popup": "popup.html"
      }
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
      }
    },

    background: function() {
      browser.runtime.onMessage.addListener(msg => {
        browser.test.assertEq(msg, "from-popup", "correct message received");
        browser.test.notifyPass("browser_action.simple");
      });
    },
  });

  yield extension.startup();

  // FIXME: Should really test opening the popup here.

  yield extension.awaitFinish("browser_action.simple");
  yield extension.unload();
});
