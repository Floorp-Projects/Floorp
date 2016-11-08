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

    background: async function() {
      let firstTab;
      let promiseResponse = new Promise(resolve => {
        browser.runtime.onMessage.addListener((msg, sender, respond) => {
          if (msg == "content-script-ready") {
            let tabId = sender.tab.id;

            Promise.all([
              promiseResponse,

              browser.tabs.sendMessage(tabId, "respond-now"),
              browser.tabs.sendMessage(tabId, "respond-now-2"),
              new Promise(resolve => browser.tabs.sendMessage(tabId, "respond-soon", resolve)),
              browser.tabs.sendMessage(tabId, "respond-promise"),
              browser.tabs.sendMessage(tabId, "respond-never"),
              new Promise(resolve => {
                browser.runtime.sendMessage("respond-never", response => { resolve(response); });
              }),

              browser.tabs.sendMessage(tabId, "respond-error").catch(error => Promise.resolve({error})),
              browser.tabs.sendMessage(tabId, "throw-error").catch(error => Promise.resolve({error})),

              browser.tabs.sendMessage(firstTab, "no-listener").catch(error => Promise.resolve({error})),
            ]).then(([response, respondNow, respondNow2, respondSoon, respondPromise, respondNever, respondNever2, respondError, throwError, noListener]) => {
              browser.test.assertEq("expected-response", response, "Content script got the expected response");

              browser.test.assertEq("respond-now", respondNow, "Got the expected immediate response");
              browser.test.assertEq("respond-now-2", respondNow2, "Got the expected immediate response from the second listener");
              browser.test.assertEq("respond-soon", respondSoon, "Got the expected delayed response");
              browser.test.assertEq("respond-promise", respondPromise, "Got the expected promise response");
              browser.test.assertEq(undefined, respondNever, "Got the expected no-response resolution");
              browser.test.assertEq(undefined, respondNever2, "Got the expected no-response resolution");

              browser.test.assertEq("respond-error", respondError.error.message, "Got the expected error response");
              browser.test.assertEq("throw-error", throwError.error.message, "Got the expected thrown error response");

              browser.test.assertEq("Could not establish connection. Receiving end does not exist.",
                                    noListener.error.message,
                                    "Got the expected no listener response");

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

      let tabs = await browser.tabs.query({currentWindow: true, active: true});
      firstTab = tabs[0].id;
      browser.tabs.create({url: "http://example.com/"});
    },

    files: {
      "content-script.js": async function() {
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

        browser.runtime.onMessage.addListener((msg, sender, respond) => {
          if (msg == "respond-now") {
            respond("hello");
          } else if (msg == "respond-now-2") {
            respond(msg);
          }
        });

        let response = await browser.runtime.sendMessage("content-script-ready");
        browser.runtime.sendMessage(["got-response", response]);
      },
    },
  });

  yield extension.startup();

  yield extension.awaitFinish("sendMessage");

  yield extension.unload();
});


add_task(function* tabsSendHidden() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],

      "content_scripts": [{
        "matches": ["http://example.com/content*"],
        "js": ["content-script.js"],
        "run_at": "document_start",
      }],
    },

    background: async function() {
      let resolveContent;
      browser.runtime.onMessage.addListener((msg, sender) => {
        if (msg[0] == "content-ready") {
          resolveContent(msg[1]);
        }
      });

      let awaitContent = url => {
        return new Promise(resolve => {
          resolveContent = resolve;
        }).then(result => {
          browser.test.assertEq(url, result, "Expected content script URL");
        });
      };

      try {
        const URL1 = "http://example.com/content1.html";
        const URL2 = "http://example.com/content2.html";

        let tab = await browser.tabs.create({url: URL1});
        await awaitContent(URL1);

        let url = await browser.tabs.sendMessage(tab.id, URL1);
        browser.test.assertEq(URL1, url, "Should get response from expected content window");

        await browser.tabs.update(tab.id, {url: URL2});
        await awaitContent(URL2);

        url = await browser.tabs.sendMessage(tab.id, URL2);
        browser.test.assertEq(URL2, url, "Should get response from expected content window");

        // Repeat once just to be sure the first message was processed by all
        // listeners before we exit the test.
        url = await browser.tabs.sendMessage(tab.id, URL2);
        browser.test.assertEq(URL2, url, "Should get response from expected content window");

        await browser.tabs.remove(tab.id);

        browser.test.notifyPass("contentscript-bfcache-window");
      } catch (error) {
        browser.test.fail(`Error: ${error} :: ${error.stack}`);
        browser.test.notifyFail("contentscript-bfcache-window");
      }
    },

    files: {
      "content-script.js": function() {
        // Store this in a local variable to make sure we don't touch any
        // properties of the possibly-hidden content window.
        let href = window.location.href;

        browser.runtime.onMessage.addListener((msg, sender) => {
          browser.test.assertEq(href, msg, "Should be in the expected content window");

          return Promise.resolve(href);
        });

        browser.runtime.sendMessage(["content-ready", href]);
      },
    },
  });

  yield extension.startup();

  yield extension.awaitFinish("contentscript-bfcache-window");

  yield extension.unload();
});


add_task(function* tabsSendMessageNoExceptionOnNonExistentTab() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    async background() {
      let url = "http://example.com/mochitest/browser/browser/components/extensions/test/browser/file_dummy.html";
      let tab = await browser.tabs.create({url});

      try {
        browser.tabs.sendMessage(tab.id, "message");
        browser.tabs.sendMessage(tab.id + 100, "message");
      } catch (e) {
        browser.test.fail("no exception should be raised on tabs.sendMessage to nonexistent tabs");
      }

      await browser.tabs.remove(tab.id);

      browser.test.notifyPass("tabs.sendMessage");
    },
  });

  yield Promise.all([
    extension.startup(),
    extension.awaitFinish("tabs.sendMessage"),
  ]);

  yield extension.unload();
});
