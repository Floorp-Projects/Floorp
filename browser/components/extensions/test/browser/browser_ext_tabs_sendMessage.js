/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* tabsSendMessageReply() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],

      "content_scripts": [{
        "matches": ["http://example.com/"],
        "js": ["content-script.js"],
        "run_at": "document_start",
      }],
    },

    background: function() {
      let promiseResponse = new Promise(resolve => {
        browser.runtime.onMessage.addListener((msg, sender, respond) => {
          if (msg == "content-script-ready") {
            let tabId = sender.tab.id;

            browser.tabs.sendMessage(tabId, "respond-never", response => {
              browser.test.fail("Got unexpected response callback");
              browser.test.notifyFail("sendMessage");
            });

            Promise.all([
              promiseResponse,
              browser.tabs.sendMessage(tabId, "respond-now"),
              new Promise(resolve => browser.tabs.sendMessage(tabId, "respond-soon", resolve)),
              browser.tabs.sendMessage(tabId, "respond-promise"),
              browser.tabs.sendMessage(tabId, "respond-never"),
              browser.tabs.sendMessage(tabId, "respond-error").catch(error => Promise.resolve({error})),
              browser.tabs.sendMessage(tabId, "throw-error").catch(error => Promise.resolve({error})),
            ]).then(([response, respondNow, respondSoon, respondPromise, respondNever, respondError, throwError]) => {
              browser.test.assertEq("expected-response", response, "Content script got the expected response");

              browser.test.assertEq("respond-now", respondNow, "Got the expected immediate response");
              browser.test.assertEq("respond-soon", respondSoon, "Got the expected delayed response");
              browser.test.assertEq("respond-promise", respondPromise, "Got the expected promise response");
              browser.test.assertEq(undefined, respondNever, "Got the expected no-response resolution");

              browser.test.assertEq("respond-error", respondError.error.message, "Got the expected error response");
              browser.test.assertEq("throw-error", throwError.error.message, "Got the expected thrown error response");

              return browser.tabs.remove(tabId);
            }).then(() => {
              browser.test.notifyPass("sendMessage");
            });

            return Promise.resolve("expected-response");
          } else if (msg[0] == "got-response") {
            resolve(msg[1]);
          }
        });
      });

      browser.tabs.create({url: "http://example.com/"});
    },

    files: {
      "content-script.js": function() {
        browser.runtime.onMessage.addListener((msg, sender, respond) => {
          if (msg == "respond-now") {
            respond(msg);
          } else if (msg == "respond-soon") {
            setTimeout(() => { respond(msg); }, 0);
            return true;
          } else if (msg == "respond-promise") {
            return Promise.resolve(msg);
          } else if (msg == "respond-never") {
            return;
          } else if (msg == "respond-error") {
            return Promise.reject(new Error(msg));
          } else if (msg == "throw-error") {
            throw new Error(msg);
          }
        });
        browser.runtime.sendMessage("content-script-ready").then(response => {
          browser.runtime.sendMessage(["got-response", response]);
        });
      },
    },
  });

  yield extension.startup();

  yield extension.awaitFinish("sendMessage");

  yield extension.unload();
});

add_task(function* tabsSendMessageNoExceptionOnNonExistentTab() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.create({url: "about:robots"}, tab => {
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
