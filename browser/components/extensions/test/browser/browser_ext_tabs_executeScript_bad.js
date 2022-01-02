"use strict";

async function testHasNoPermission(params) {
  let contentSetup = params.contentSetup || (() => Promise.resolve());

  async function background(contentSetup) {
    browser.test.onMessage.addListener(async msg => {
      browser.test.assertEq(msg, "execute-script");

      await browser.test.assertRejects(
        browser.tabs.executeScript({
          file: "script.js",
        }),
        /Missing host permission for the tab/
      );

      browser.test.notifyPass("executeScript");
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
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  if (params.setup) {
    await params.setup(extension);
  }

  extension.sendMessage("execute-script");

  await extension.awaitFinish("executeScript");
  await extension.unload();
}

add_task(async function testBadPermissions() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888/"
  );

  info("Test no special permissions");
  await testHasNoPermission({
    manifest: { permissions: [] },
  });

  info("Test tabs permissions");
  await testHasNoPermission({
    manifest: { permissions: ["tabs"] },
  });

  info("Test no special permissions, commands, key press");
  await testHasNoPermission({
    manifest: {
      permissions: [],
      commands: {
        "test-tabs-executeScript": {
          suggested_key: {
            default: "Alt+Shift+K",
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
    setup: async function(extension) {
      await EventUtils.synthesizeKey("k", { altKey: true, shiftKey: true });
      await extension.awaitMessage("tabs-command-key-pressed");
    },
  });

  info("Test no special permissions, _execute_browser_action command");
  await testHasNoPermission({
    manifest: {
      permissions: [],
      browser_action: {},
      commands: {
        _execute_browser_action: {
          suggested_key: {
            default: "Alt+Shift+K",
          },
        },
      },
    },
    contentSetup: function() {
      browser.browserAction.onClicked.addListener(() => {
        browser.test.sendMessage("tabs-command-key-pressed");
      });
      return Promise.resolve();
    },
    setup: async function(extension) {
      await EventUtils.synthesizeKey("k", { altKey: true, shiftKey: true });
      await extension.awaitMessage("tabs-command-key-pressed");
    },
  });

  info("Test no special permissions, _execute_page_action command");
  await testHasNoPermission({
    manifest: {
      permissions: [],
      page_action: {},
      commands: {
        _execute_page_action: {
          suggested_key: {
            default: "Alt+Shift+K",
          },
        },
      },
    },
    contentSetup: async function() {
      browser.pageAction.onClicked.addListener(() => {
        browser.test.sendMessage("tabs-command-key-pressed");
      });
      let [tab] = await browser.tabs.query({
        active: true,
        currentWindow: true,
      });
      await browser.pageAction.show(tab.id);
    },
    setup: async function(extension) {
      await EventUtils.synthesizeKey("k", { altKey: true, shiftKey: true });
      await extension.awaitMessage("tabs-command-key-pressed");
    },
  });

  info("Test active tab, commands, no key press");
  await testHasNoPermission({
    manifest: {
      permissions: ["activeTab"],
      commands: {
        "test-tabs-executeScript": {
          suggested_key: {
            default: "Alt+Shift+K",
          },
        },
      },
    },
  });

  info("Test active tab, browser action, no click");
  await testHasNoPermission({
    manifest: {
      permissions: ["activeTab"],
      browser_action: {},
    },
  });

  info("Test active tab, page action, no click");
  await testHasNoPermission({
    manifest: {
      permissions: ["activeTab"],
      page_action: {},
    },
    contentSetup: async function() {
      let [tab] = await browser.tabs.query({
        active: true,
        currentWindow: true,
      });
      await browser.pageAction.show(tab.id);
    },
  });

  BrowserTestUtils.removeTab(tab);
});

add_task(async function testMatchDataURI() {
  // allow top level data: URI navigations, otherwise
  // window.location.href = data: would be blocked
  await SpecialPowers.pushPrefEnv({
    set: [["security.data_uri.block_toplevel_data_uri_navigations", false]],
  });

  const target = ExtensionTestUtils.loadExtension({
    files: {
      "page.html": `<!DOCTYPE html>
        <meta charset="utf-8">
        <script src="page.js"></script>
        <iframe id="inherited" src="data:text/html;charset=utf-8,inherited"></iframe>
      `,
      "page.js": function() {
        browser.test.onMessage.addListener((msg, url) => {
          if (msg !== "navigate") {
            return;
          }
          window.location.href = url;
        });
      },
    },
    background() {
      browser.tabs.create({
        active: true,
        url: browser.runtime.getURL("page.html"),
      });
    },
  });

  const scripts = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["<all_urls>", "webNavigation"],
    },
    background() {
      browser.webNavigation.onCompleted.addListener(({ url, frameId }) => {
        browser.test.log(`Document loading complete: ${url}`);
        if (frameId === 0) {
          browser.test.sendMessage("tab-ready", url);
        }
      });

      browser.test.onMessage.addListener(async msg => {
        if (msg !== "execute") {
          return;
        }
        await browser.test.assertRejects(
          browser.tabs.executeScript({
            code: "location.href;",
            allFrames: true,
          }),
          /Missing host permission/,
          "Should not execute in `data:` frame"
        );

        browser.test.sendMessage("done");
      });
    },
  });

  await scripts.startup();
  await target.startup();

  // Test extension page with a data: iframe.
  const page = await scripts.awaitMessage("tab-ready");
  ok(page.endsWith("page.html"), "Extension page loaded into a tab");

  scripts.sendMessage("execute");
  await scripts.awaitMessage("done");

  // Test extension tab navigated to a data: URI.
  const data = "data:text/html;charset=utf-8,also-inherits";
  target.sendMessage("navigate", data);

  const url = await scripts.awaitMessage("tab-ready");
  is(url, data, "Extension tab navigated to a data: URI");

  scripts.sendMessage("execute");
  await scripts.awaitMessage("done");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await scripts.unload();
  await target.unload();
});

add_task(async function testBadURL() {
  async function background() {
    let promises = [
      new Promise(resolve => {
        browser.tabs.executeScript(
          {
            file: "http://example.com/script.js",
          },
          result => {
            browser.test.assertEq(undefined, result, "Result value");

            browser.test.assertTrue(
              browser.runtime.lastError instanceof Error,
              "runtime.lastError is Error"
            );

            browser.test.assertTrue(
              browser.runtime.lastError instanceof Error,
              "runtime.lastError is Error"
            );

            browser.test.assertEq(
              "Files to be injected must be within the extension",
              browser.runtime.lastError && browser.runtime.lastError.message,
              "runtime.lastError value"
            );

            browser.test.assertEq(
              "Files to be injected must be within the extension",
              browser.runtime.lastError && browser.runtime.lastError.message,
              "runtime.lastError value"
            );

            resolve();
          }
        );
      }),

      browser.tabs
        .executeScript({
          file: "http://example.com/script.js",
        })
        .catch(error => {
          browser.test.assertTrue(error instanceof Error, "Error is Error");

          browser.test.assertEq(
            null,
            browser.runtime.lastError,
            "runtime.lastError value"
          );

          browser.test.assertEq(
            null,
            browser.runtime.lastError,
            "runtime.lastError value"
          );

          browser.test.assertEq(
            "Files to be injected must be within the extension",
            error && error.message,
            "error value"
          );
        }),
    ];

    await Promise.all(promises);

    browser.test.notifyPass("executeScript-lastError");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["<all_urls>"],
    },

    background,
  });

  await extension.startup();

  await extension.awaitFinish("executeScript-lastError");

  await extension.unload();
});

// TODO bug 1435100: Test that |executeScript| fails if the tab has navigated
// to a new page, and no longer matches our expected state. This involves
// intentionally trying to trigger a race condition.

add_task(forceGC);
