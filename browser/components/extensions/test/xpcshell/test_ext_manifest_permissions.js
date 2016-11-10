/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* globals chrome */

function* testPermission(options) {
  function background(options) {
    browser.test.sendMessage("typeof-namespace", {
      browser: typeof browser[options.namespace],
      chrome: typeof chrome[options.namespace],
    });
  }

  let extensionDetails = {
    background: `(${background})(${JSON.stringify(options)})`,
  };

  let extension = ExtensionTestUtils.loadExtension(extensionDetails);

  yield extension.startup();

  let types = yield extension.awaitMessage("typeof-namespace");
  equal(types.browser, "undefined", `Type of browser.${options.namespace} without manifest entry`);
  equal(types.chrome, "undefined", `Type of chrome.${options.namespace} without manifest entry`);

  yield extension.unload();

  extensionDetails.manifest = options.manifest;
  extension = ExtensionTestUtils.loadExtension(extensionDetails);

  yield extension.startup();

  types = yield extension.awaitMessage("typeof-namespace");
  equal(types.browser, "object", `Type of browser.${options.namespace} with manifest entry`);
  equal(types.chrome, "object", `Type of chrome.${options.namespace} with manifest entry`);

  yield extension.unload();
}

add_task(function* test_browserAction() {
  yield testPermission({
    namespace: "browserAction",
    manifest: {
      browser_action: {},
    },
  });
});

add_task(function* test_pageAction() {
  yield testPermission({
    namespace: "pageAction",
    manifest: {
      page_action: {},
    },
  });
});
