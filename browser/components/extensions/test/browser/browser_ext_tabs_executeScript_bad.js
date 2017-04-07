"use strict";

// This is a pretty terrible hack, but it's the best we can do until we
// support |executeScript| callbacks and |lastError|.
function* testHasNoPermission(params) {
  let contentSetup = params.contentSetup || (() => Promise.resolve());

  async function background(contentSetup) {
    browser.runtime.onMessage.addListener((msg, sender) => {
      browser.test.assertEq(msg, "second script ran", "second script ran");
      browser.test.notifyPass("executeScript");
    });

    browser.test.onMessage.addListener(msg => {
      browser.test.assertEq(msg, "execute-script");

      browser.tabs.query({currentWindow: true}, tabs => {
        browser.tabs.executeScript({
          file: "script.js",
        });

        // Execute a script we know we have permissions for in the
        // second tab, in the hopes that it will execute after the
        // first one. This has intermittent failure written all over
        // it, but it's just about the best we can do until we
        // support callbacks for executeScript.
        browser.tabs.executeScript(tabs[1].id, {
          file: "second-script.js",
        });
      });
    });

    await contentSetup();

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: params.manifest,

    background: `(${background})(${contentSetup})`,

    files: {
      "script.js": function() {
        browser.runtime.sendMessage("first script ran");
      },

      "second-script.js": function() {
        browser.runtime.sendMessage("second script ran");
      },
    },
  });

  yield extension.startup();
  yield extension.awaitMessage("ready");

  if (params.setup) {
    yield params.setup(extension);
  }

  extension.sendMessage("execute-script");

  yield extension.awaitFinish("executeScript");
  yield extension.unload();
}

add_task(function* testBadPermissions() {
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  info("Test no special permissions");
  yield testHasNoPermission({
    manifest: {"permissions": ["http://example.com/"]},
  });

  info("Test tabs permissions");
  yield testHasNoPermission({
    manifest: {"permissions": ["http://example.com/", "tabs"]},
  });

  info("Test no special permissions, commands, key press");
  yield testHasNoPermission({
    manifest: {
      "permissions": ["http://example.com/"],
      "commands": {
        "test-tabs-executeScript": {
          "suggested_key": {
            "default": "Alt+Shift+K",
          },
        },
      },
    },
    contentSetup: function() {
      browser.commands.onCommand.addListener(function(command) {
        if (command == "test-tabs-executeScript") {
          browser.test.sendMessage("tabs-command-key-pressed");
        }
      });
      return Promise.resolve();
    },
    setup: function* (extension) {
      yield EventUtils.synthesizeKey("k", {altKey: true, shiftKey: true});
      yield extension.awaitMessage("tabs-command-key-pressed");
    },
  });

  info("Test active tab, commands, no key press");
  yield testHasNoPermission({
    manifest: {
      "permissions": ["http://example.com/", "activeTab"],
      "commands": {
        "test-tabs-executeScript": {
          "suggested_key": {
            "default": "Alt+Shift+K",
          },
        },
      },
    },
  });

  info("Test active tab, browser action, no click");
  yield testHasNoPermission({
    manifest: {
      "permissions": ["http://example.com/", "activeTab"],
      "browser_action": {},
    },
  });

  info("Test active tab, page action, no click");
  yield testHasNoPermission({
    manifest: {
      "permissions": ["http://example.com/", "activeTab"],
      "page_action": {},
    },
    contentSetup: async function() {
      let [tab] = await browser.tabs.query({active: true, currentWindow: true});
      await browser.pageAction.show(tab.id);
    },
  });

  yield BrowserTestUtils.removeTab(tab2);
  yield BrowserTestUtils.removeTab(tab1);
});

add_task(function* testMatchDataURI() {
  const target = ExtensionTestUtils.loadExtension({
    files: {
      "page.html": `<!DOCTYPE html>
        <meta charset="utf-8">
        <script src="page.js"></script>
        <iframe id="inherited" src="data:text/html;charset=utf-8,inherited"></iframe>
      `,
      "page.js": function() {
        browser.test.onMessage.addListener((msg, url) => {
          window.location.href = url;
        });
      },
    },
    background() {
      browser.tabs.create({active: true, url: browser.runtime.getURL("page.html")});
    },
  });

  const scripts = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["<all_urls>", "webNavigation"],
    },
    background() {
      browser.webNavigation.onCompleted.addListener(({url, frameId}) => {
        browser.test.log(`Document loading complete: ${url}`);
        if (frameId === 0) {
          browser.test.sendMessage("tab-ready", url);
        }
      });

      browser.test.onMessage.addListener(async msg => {
        browser.test.assertRejects(
          browser.tabs.executeScript({
            code: "location.href;",
            allFrames: true,
          }),
          /Missing host permission/,
          "Should not execute in `data:` frame");

        browser.test.sendMessage("done");
      });
    },
  });

  yield scripts.startup();
  yield target.startup();

  // Test extension page with a data: iframe.
  const page = yield scripts.awaitMessage("tab-ready");
  ok(page.endsWith("page.html"), "Extension page loaded into a tab");

  scripts.sendMessage("execute");
  yield scripts.awaitMessage("done");

  // Test extension tab navigated to a data: URI.
  const data = "data:text/html;charset=utf-8,also-inherits";
  target.sendMessage("navigate", data);

  const url = yield scripts.awaitMessage("tab-ready");
  is(url, data, "Extension tab navigated to a data: URI");

  scripts.sendMessage("execute");
  yield scripts.awaitMessage("done");

  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
  yield scripts.unload();
  yield target.unload();
});

add_task(function* testBadURL() {
  async function background() {
    let promises = [
      new Promise(resolve => {
        browser.tabs.executeScript({
          file: "http://example.com/script.js",
        }, result => {
          browser.test.assertEq(undefined, result, "Result value");

          browser.test.assertTrue(browser.extension.lastError instanceof Error,
                                  "runtime.lastError is Error");

          browser.test.assertTrue(browser.runtime.lastError instanceof Error,
                                  "runtime.lastError is Error");

          browser.test.assertEq(
            "Files to be injected must be within the extension",
            browser.extension.lastError && browser.extension.lastError.message,
            "extension.lastError value");

          browser.test.assertEq(
            "Files to be injected must be within the extension",
            browser.runtime.lastError && browser.runtime.lastError.message,
            "runtime.lastError value");

          resolve();
        });
      }),

      browser.tabs.executeScript({
        file: "http://example.com/script.js",
      }).catch(error => {
        browser.test.assertTrue(error instanceof Error, "Error is Error");

        browser.test.assertEq(null, browser.extension.lastError,
                              "extension.lastError value");

        browser.test.assertEq(null, browser.runtime.lastError,
                              "runtime.lastError value");

        browser.test.assertEq(
          "Files to be injected must be within the extension",
          error && error.message,
          "error value");
      }),
    ];

    await Promise.all(promises);

    browser.test.notifyPass("executeScript-lastError");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["<all_urls>"],
    },

    background,
  });

  yield extension.startup();

  yield extension.awaitFinish("executeScript-lastError");

  yield extension.unload();
});

// TODO: Test that |executeScript| fails if the tab has navigated to a
// new page, and no longer matches our expected state. This involves
// intentionally trying to trigger a race condition, and is probably not
// even worth attempting until we have proper |executeScript| callbacks.

add_task(forceGC);
