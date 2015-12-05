/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* tabsSendMessageNoExceptionOnNonExistentTab() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.create({ url: "about:robots"}, tab => {
        let exception;
        try {
          browser.tabs.sendMessage(tab.id, "message");
          browser.tabs.sendMessage(tab.id + 100, "message");
        } catch (e) {
          exception = e;
        }

        browser.test.assertEq(undefined, exception, "no exception should be raised on tabs.sendMessage to unexistent tabs");
        browser.tabs.remove(tab.id, function() {
          browser.test.notifyPass("tabs.sendMessage");
        });
      });
    },
  });

  yield Promise.all([
    extension.startup(),
    extension.awaitFinish("tabs.sendMessage"),
  ]);

  yield extension.unload();
});
