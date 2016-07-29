/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testExecuteScript() {
  let {MessageChannel} = Cu.import("resource://gre/modules/MessageChannel.jsm", {});

  let messageManagersSize = MessageChannel.messageManagers.size;
  let responseManagersSize = MessageChannel.responseManagers.size;

  const BASE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/";
  const URL = BASE + "file_iframe_document.html";
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, URL, true);

  function background() {
    browser.tabs.query({active: true, currentWindow: true}).then(tabs => {
      return browser.webNavigation.getAllFrames({tabId: tabs[0].id});
    }).then(frames => {
      browser.test.log(`FRAMES: ${frames[1].frameId} ${JSON.stringify(frames)}\n`);
      return Promise.all([
        browser.tabs.executeScript({
          code: "42",
        }).then(result => {
          browser.test.assertEq(42, result, "Expected callback result");
        }),

        browser.tabs.executeScript({
          file: "script.js",
          code: "42",
        }).then(result => {
          browser.test.fail("Expected not to be able to execute a script with both file and code");
        }, error => {
          browser.test.assertTrue(/a 'code' or a 'file' property, but not both/.test(error.message),
                                  "Got expected error");
        }),

        browser.tabs.executeScript({
          file: "script.js",
        }).then(result => {
          browser.test.assertEq(undefined, result, "Expected callback result");
        }),

        browser.tabs.executeScript({
          file: "script2.js",
        }).then(result => {
          browser.test.assertEq(27, result, "Expected callback result");
        }),

        browser.tabs.executeScript({
          code: "location.href;",
          allFrames: true,
        }).then(result => {
          browser.test.assertTrue(Array.isArray(result), "Result is an array");

          browser.test.assertEq(2, result.length, "Result has correct length");

          browser.test.assertTrue(/\/file_iframe_document\.html$/.test(result[0]), "First result is correct");
          browser.test.assertEq("http://mochi.test:8888/", result[1], "Second result is correct");
        }),

        browser.tabs.executeScript({
          code: "location.href;",
          runAt: "document_end",
        }).then(result => {
          browser.test.assertTrue(typeof(result) == "string", "Result is a string");

          browser.test.assertTrue(/\/file_iframe_document\.html$/.test(result), "Result is correct");
        }),

        browser.tabs.executeScript({
          code: "window",
        }).then(result => {
          browser.test.fail("Expected error when returning non-structured-clonable object");
        }, error => {
          browser.test.assertEq("Script returned non-structured-clonable data",
                                error.message, "Got expected error");
        }),

        browser.tabs.executeScript({
          code: "Promise.resolve(window)",
        }).then(result => {
          browser.test.fail("Expected error when returning non-structured-clonable object");
        }, error => {
          browser.test.assertEq("Script returned non-structured-clonable data",
                                error.message, "Got expected error");
        }),

        browser.tabs.executeScript({
          frameId: Number.MAX_SAFE_INTEGER,
          code: "42",
        }).then(result => {
          browser.test.fail("Expected error when specifying invalid frame ID");
        }, error => {
          let details = {
            frame_id: Number.MAX_SAFE_INTEGER,
            matchesHost: ["http://mochi.test/", "http://example.com/"],
          };
          browser.test.assertEq(`No window matching ${JSON.stringify(details)}`,
                                error.message, "Got expected error");
        }),

        browser.tabs.create({url: "http://example.net/", active: false}).then(tab => {
          return browser.tabs.executeScript(tab.id, {
            code: "42",
          }).then(result => {
            browser.test.fail("Expected error when trying to execute on invalid domain");
          }, error => {
            let details = {
              matchesHost: ["http://mochi.test/", "http://example.com/"],
            };
            browser.test.assertEq(`No window matching ${JSON.stringify(details)}`,
                                  error.message, "Got expected error");
          }).then(() => {
            return browser.tabs.remove(tab.id);
          });
        }),

        browser.tabs.executeScript({
          code: "Promise.resolve(42)",
        }).then(result => {
          browser.test.assertEq(42, result, "Got expected promise resolution value as result");
        }),

        browser.tabs.executeScript({
          code: "location.href;",
          runAt: "document_end",
          allFrames: true,
        }).then(result => {
          browser.test.assertTrue(Array.isArray(result), "Result is an array");

          browser.test.assertEq(2, result.length, "Result has correct length");

          browser.test.assertTrue(/\/file_iframe_document\.html$/.test(result[0]), "First result is correct");
          browser.test.assertEq("http://mochi.test:8888/", result[1], "Second result is correct");
        }),

        browser.tabs.executeScript({
          code: "location.href;",
          frameId: frames[0].frameId,
        }).then(result => {
          browser.test.assertTrue(/\/file_iframe_document\.html$/.test(result), `Result for frameId[0] is correct: ${result}`);
        }),

        browser.tabs.executeScript({
          code: "location.href;",
          frameId: frames[1].frameId,
        }).then(result => {
          browser.test.assertEq("http://mochi.test:8888/", result, "Result for frameId[1] is correct");
        }),

        browser.tabs.create({url: "http://example.com/"}).then(tab => {
          return browser.tabs.executeScript(tab.id, {code: "location.href"}).then(result => {
            browser.test.assertEq("http://example.com/", result, "Script executed correctly in new tab");

            return browser.tabs.remove(tab.id);
          });
        }),

        new Promise(resolve => {
          browser.runtime.onMessage.addListener(message => {
            browser.test.assertEq("script ran", message, "Expected runtime message");
            resolve();
          });
        }),
      ]);
    }).then(() => {
      browser.test.notifyPass("executeScript");
    }).catch(e => {
      browser.test.fail(`Error: ${e} :: ${e.stack}`);
      browser.test.notifyFail("executeScript");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["http://mochi.test/", "http://example.com/", "webNavigation"],
    },

    background,

    files: {
      "script.js": function() {
        browser.runtime.sendMessage("script ran");
      },

      "script2.js": "27",
    },
  });

  yield extension.startup();

  yield extension.awaitFinish("executeScript");

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);

  // Make sure that we're not holding on to references to closed message
  // managers.
  is(MessageChannel.messageManagers.size, messageManagersSize, "Message manager count");
  is(MessageChannel.responseManagers.size, responseManagersSize, "Response manager count");
  is(MessageChannel.pendingResponses.size, 0, "Pending response count");
});
