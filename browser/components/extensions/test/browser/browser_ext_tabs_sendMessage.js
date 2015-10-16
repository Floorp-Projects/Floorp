add_task(function* tabsSendMessageNoExceptionOnNonExistentTab() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"]
    },

    background: function() {
      chrome.tabs.create({ url: "about:robots"}, function (tab) {
        var exception;
        try {
          browser.tabs.sendMessage(tab.id, "message");
          browser.tabs.sendMessage(tab.id + 100, "message");
        } catch(e) {
          exception = e;
        }

        browser.test.assertEq(undefined, exception, "no exception should be raised on tabs.sendMessage to unexistent tabs");
        chrome.tabs.remove(tab.id, function() {
          browser.test.notifyPass("tabs.sendMessage");
        })
      })
    },
  });

  yield Promise.all([
    extension.startup(),
    extension.awaitFinish("tabs.sendMessage")
  ]);

  yield extension.unload();
});
