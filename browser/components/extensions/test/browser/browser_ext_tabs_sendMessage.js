/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function tabsSendMessageReply() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],

      content_scripts: [
        {
          matches: ["http://example.com/"],
          js: ["content-script.js"],
          run_at: "document_start",
        },
      ],
    },

    background: async function () {
      let firstTab;
      let promiseResponse = new Promise(resolve => {
        browser.runtime.onMessage.addListener((msg, sender) => {
          if (msg == "content-script-ready") {
            let tabId = sender.tab.id;

            Promise.all([
              promiseResponse,

              browser.tabs.sendMessage(tabId, "respond-now"),
              browser.tabs.sendMessage(tabId, "respond-now-2"),
              new Promise(resolve =>
                browser.tabs.sendMessage(tabId, "respond-soon", resolve)
              ),
              browser.tabs.sendMessage(tabId, "respond-promise"),
              browser.tabs.sendMessage(tabId, "respond-promise-false"),
              browser.tabs.sendMessage(tabId, "respond-false"),
              browser.tabs.sendMessage(tabId, "respond-never"),
              new Promise(resolve => {
                browser.runtime.sendMessage("respond-never", response => {
                  resolve(response);
                });
              }),

              browser.tabs
                .sendMessage(tabId, "respond-error")
                .catch(error => Promise.resolve({ error })),
              browser.tabs
                .sendMessage(tabId, "throw-error")
                .catch(error => Promise.resolve({ error })),

              browser.tabs
                .sendMessage(tabId, "respond-uncloneable")
                .catch(error => Promise.resolve({ error })),
              browser.tabs
                .sendMessage(tabId, "reject-uncloneable")
                .catch(error => Promise.resolve({ error })),
              browser.tabs
                .sendMessage(tabId, "reject-undefined")
                .catch(error => Promise.resolve({ error })),
              browser.tabs
                .sendMessage(tabId, "throw-undefined")
                .catch(error => Promise.resolve({ error })),

              browser.tabs
                .sendMessage(firstTab, "no-listener")
                .catch(error => Promise.resolve({ error })),
            ])
              .then(
                ([
                  response,
                  respondNow,
                  respondNow2,
                  respondSoon,
                  respondPromise,
                  respondPromiseFalse,
                  respondFalse,
                  respondNever,
                  respondNever2,
                  respondError,
                  throwError,
                  respondUncloneable,
                  rejectUncloneable,
                  rejectUndefined,
                  throwUndefined,
                  noListener,
                ]) => {
                  browser.test.assertEq(
                    "expected-response",
                    response,
                    "Content script got the expected response"
                  );

                  browser.test.assertEq(
                    "respond-now",
                    respondNow,
                    "Got the expected immediate response"
                  );
                  browser.test.assertEq(
                    "respond-now-2",
                    respondNow2,
                    "Got the expected immediate response from the second listener"
                  );
                  browser.test.assertEq(
                    "respond-soon",
                    respondSoon,
                    "Got the expected delayed response"
                  );
                  browser.test.assertEq(
                    "respond-promise",
                    respondPromise,
                    "Got the expected promise response"
                  );
                  browser.test.assertEq(
                    false,
                    respondPromiseFalse,
                    "Got the expected false value as a promise result"
                  );
                  browser.test.assertEq(
                    undefined,
                    respondFalse,
                    "Got the expected no-response when onMessage returns false"
                  );
                  browser.test.assertEq(
                    undefined,
                    respondNever,
                    "Got the expected no-response resolution"
                  );
                  browser.test.assertEq(
                    undefined,
                    respondNever2,
                    "Got the expected no-response resolution"
                  );

                  browser.test.assertEq(
                    "respond-error",
                    respondError.error.message,
                    "Got the expected error response"
                  );
                  browser.test.assertEq(
                    "throw-error",
                    throwError.error.message,
                    "Got the expected thrown error response"
                  );

                  browser.test.assertEq(
                    "Could not establish connection. Receiving end does not exist.",
                    respondUncloneable.error.message,
                    "An uncloneable response should be ignored"
                  );
                  browser.test.assertEq(
                    "An unexpected error occurred",
                    rejectUncloneable.error.message,
                    "Got the expected error for a rejection with an uncloneable value"
                  );
                  browser.test.assertEq(
                    "An unexpected error occurred",
                    rejectUndefined.error.message,
                    "Got the expected error for a void rejection"
                  );
                  browser.test.assertEq(
                    "An unexpected error occurred",
                    throwUndefined.error.message,
                    "Got the expected error for a void throw"
                  );

                  browser.test.assertEq(
                    "Could not establish connection. Receiving end does not exist.",
                    noListener.error.message,
                    "Got the expected no listener response"
                  );

                  return browser.tabs.remove(tabId);
                }
              )
              .then(() => {
                browser.test.notifyPass("sendMessage");
              });

            return Promise.resolve("expected-response");
          } else if (msg[0] == "got-response") {
            resolve(msg[1]);
          }
        });
      });

      let tabs = await browser.tabs.query({
        currentWindow: true,
        active: true,
      });
      firstTab = tabs[0].id;
      browser.tabs.create({ url: "http://example.com/" });
    },

    files: {
      "content-script.js": async function () {
        browser.runtime.onMessage.addListener((msg, sender, respond) => {
          if (msg == "respond-now") {
            respond(msg);
          } else if (msg == "respond-soon") {
            setTimeout(() => {
              respond(msg);
            }, 0);
            return true;
          } else if (msg == "respond-promise") {
            return Promise.resolve(msg);
          } else if (msg == "respond-promise-false") {
            return Promise.resolve(false);
          } else if (msg == "respond-false") {
            // return false means that respond() is not expected to be called.
            setTimeout(() => respond("should be ignored"));
            return false;
          } else if (msg == "respond-never") {
            return undefined;
          } else if (msg == "respond-error") {
            return Promise.reject(new Error(msg));
          } else if (msg === "respond-uncloneable") {
            return Promise.resolve(window);
          } else if (msg === "reject-uncloneable") {
            return Promise.reject(window);
          } else if (msg == "reject-undefined") {
            return Promise.reject();
          } else if (msg == "throw-undefined") {
            throw undefined; // eslint-disable-line no-throw-literal
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

        let response = await browser.runtime.sendMessage(
          "content-script-ready"
        );
        browser.runtime.sendMessage(["got-response", response]);
      },
    },
  });

  await extension.startup();

  await extension.awaitFinish("sendMessage");

  await extension.unload();
});

add_task(async function tabsSendHidden() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],

      content_scripts: [
        {
          matches: ["http://example.com/content*"],
          js: ["content-script.js"],
          run_at: "document_start",
        },
      ],
    },

    background: async function () {
      let resolveContent;
      browser.runtime.onMessage.addListener(msg => {
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

        let tab = await browser.tabs.create({ url: URL1 });
        await awaitContent(URL1);

        let url = await browser.tabs.sendMessage(tab.id, URL1);
        browser.test.assertEq(
          URL1,
          url,
          "Should get response from expected content window"
        );

        await browser.tabs.update(tab.id, { url: URL2 });
        await awaitContent(URL2);

        url = await browser.tabs.sendMessage(tab.id, URL2);
        browser.test.assertEq(
          URL2,
          url,
          "Should get response from expected content window"
        );

        // Repeat once just to be sure the first message was processed by all
        // listeners before we exit the test.
        url = await browser.tabs.sendMessage(tab.id, URL2);
        browser.test.assertEq(
          URL2,
          url,
          "Should get response from expected content window"
        );

        await browser.tabs.remove(tab.id);

        browser.test.notifyPass("contentscript-bfcache-window");
      } catch (error) {
        browser.test.fail(`Error: ${error} :: ${error.stack}`);
        browser.test.notifyFail("contentscript-bfcache-window");
      }
    },

    files: {
      "content-script.js": function () {
        // Store this in a local variable to make sure we don't touch any
        // properties of the possibly-hidden content window.
        let href = window.location.href;

        browser.runtime.onMessage.addListener(msg => {
          browser.test.assertEq(
            href,
            msg,
            "Should be in the expected content window"
          );

          return Promise.resolve(href);
        });

        browser.runtime.sendMessage(["content-ready", href]);
      },
    },
  });

  await extension.startup();

  await extension.awaitFinish("contentscript-bfcache-window");

  await extension.unload();
});

add_task(async function tabsSendMessageNoExceptionOnNonExistentTab() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    async background() {
      let url =
        "http://example.com/mochitest/browser/browser/components/extensions/test/browser/file_dummy.html";
      let tab = await browser.tabs.create({ url });

      await browser.test.assertRejects(
        browser.tabs.sendMessage(tab.id, "message"),
        /Could not establish connection. Receiving end does not exist./,
        "exception should be raised on tabs.sendMessage to nonexistent tab"
      );

      await browser.test.assertRejects(
        browser.tabs.sendMessage(tab.id + 100, "message"),
        /Could not establish connection. Receiving end does not exist./,
        "exception should be raised on tabs.sendMessage to nonexistent tab"
      );

      await browser.tabs.remove(tab.id);

      browser.test.notifyPass("tabs.sendMessage");
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.sendMessage");

  await extension.unload();
});
