add_task(function* () {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"]
    },

    background: function() {
      browser.runtime.onMessage.addListener((msg, sender) => {
        browser.test.assertEq(msg, "script ran", "script ran");
        browser.test.notifyPass("executeScript");
      });

      browser.tabs.executeScript({
        file: "script.js"
      });
    },

    files: {
      "script.js": function() {
        browser.runtime.sendMessage("script ran");
      }
    }
  });

  yield extension.startup();
  yield extension.awaitFinish("executeScript");
  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);
});
