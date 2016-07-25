/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

requestLongerTimeout(2);

function* testHasPermission(params) {
  let contentSetup = params.contentSetup || (() => Promise.resolve());

  function background(contentSetup) {
    browser.runtime.onMessage.addListener((msg, sender) => {
      browser.test.assertEq(msg, "script ran", "script ran");
      browser.test.notifyPass("executeScript");
    });

    browser.test.onMessage.addListener(msg => {
      browser.test.assertEq(msg, "execute-script");

      browser.tabs.executeScript({
        file: "script.js",
      });
    });

    contentSetup().then(() => {
      browser.test.sendMessage("ready");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: params.manifest,

    background: `(${background})(${contentSetup})`,

    files: {
      "script.js": function() {
        browser.runtime.sendMessage("script ran");
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

  if (params.tearDown) {
    yield params.tearDown(extension);
  }

  yield extension.unload();
}

add_task(function* testGoodPermissions() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/", true);

  info("Test explicit host permission");
  yield testHasPermission({
    manifest: {"permissions": ["http://mochi.test/"]},
  });

  info("Test explicit host subdomain permission");
  yield testHasPermission({
    manifest: {"permissions": ["http://*.mochi.test/"]},
  });

  info("Test explicit <all_urls> permission");
  yield testHasPermission({
    manifest: {"permissions": ["<all_urls>"]},
  });

  info("Test activeTab permission with a command key press");
  yield testHasPermission({
    manifest: {
      "permissions": ["activeTab"],
      "commands": {
        "test-tabs-executeScript": {
          "suggested_key": {
            "default": "Alt+Shift+K",
          },
        },
      },
    },
    contentSetup() {
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

  info("Test activeTab permission with a browser action click");
  yield testHasPermission({
    manifest: {
      "permissions": ["activeTab"],
      "browser_action": {},
    },
    contentSetup() {
      browser.browserAction.onClicked.addListener(() => {
        browser.test.log("Clicked.");
      });
      return Promise.resolve();
    },
    setup: clickBrowserAction,
    tearDown: closeBrowserAction,
  });

  info("Test activeTab permission with a page action click");
  yield testHasPermission({
    manifest: {
      "permissions": ["activeTab"],
      "page_action": {},
    },
    contentSetup() {
      return new Promise(resolve => {
        browser.tabs.query({active: true, currentWindow: true}, tabs => {
          browser.pageAction.show(tabs[0].id).then(() => {
            resolve();
          });
        });
      });
    },
    setup: clickPageAction,
    tearDown: closePageAction,
  });

  info("Test activeTab permission with a browser action w/popup click");
  yield testHasPermission({
    manifest: {
      "permissions": ["activeTab"],
      "browser_action": {"default_popup": "_blank.html"},
    },
    setup: clickBrowserAction,
    tearDown: closeBrowserAction,
  });

  info("Test activeTab permission with a page action w/popup click");
  yield testHasPermission({
    manifest: {
      "permissions": ["activeTab"],
      "page_action": {"default_popup": "_blank.html"},
    },
    contentSetup() {
      return new Promise(resolve => {
        browser.tabs.query({active: true, currentWindow: true}, tabs => {
          browser.pageAction.show(tabs[0].id).then(() => {
            resolve();
          });
        });
      });
    },
    setup: clickPageAction,
    tearDown: closePageAction,
  });

  info("Test activeTab permission with a context menu click");
  yield testHasPermission({
    manifest: {
      "permissions": ["activeTab", "contextMenus"],
    },
    contentSetup() {
      browser.contextMenus.create({title: "activeTab", contexts: ["all"]});
      return Promise.resolve();
    },
    setup: function* (extension) {
      let contextMenu = document.getElementById("contentAreaContextMenu");
      let awaitPopupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
      let awaitPopupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");

      yield BrowserTestUtils.synthesizeMouseAtCenter("a[href]", {type: "contextmenu", button: 2},
                                                     gBrowser.selectedBrowser);
      yield awaitPopupShown;

      let item = contextMenu.querySelector("[label=activeTab]");

      yield EventUtils.synthesizeMouseAtCenter(item, {}, window);

      yield awaitPopupHidden;
    },
  });

  yield BrowserTestUtils.removeTab(tab);
});

add_task(forceGC);
