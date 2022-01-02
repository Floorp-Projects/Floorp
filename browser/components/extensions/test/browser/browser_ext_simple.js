/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_simple() {
  let extensionData = {
    manifest: {
      name: "Simple extension test",
      version: "1.0",
      manifest_version: 2,
      description: "",
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  info("load complete");
  await extension.startup();
  info("startup complete");
  await extension.unload();
  info("extension unloaded successfully");
});

add_task(async function test_background() {
  function backgroundScript() {
    browser.test.log("running background script");

    browser.test.onMessage.addListener((x, y) => {
      browser.test.assertEq(x, 10, "x is 10");
      browser.test.assertEq(y, 20, "y is 20");

      browser.test.notifyPass("background test passed");
    });

    browser.test.sendMessage("running", 1);
  }

  let extensionData = {
    background: "(" + backgroundScript.toString() + ")()",
    manifest: {
      name: "Simple extension test",
      version: "1.0",
      manifest_version: 2,
      description: "",
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  info("load complete");
  let [, x] = await Promise.all([
    extension.startup(),
    extension.awaitMessage("running"),
  ]);
  is(x, 1, "got correct value from extension");
  info("startup complete");
  extension.sendMessage(10, 20);
  await extension.awaitFinish();
  info("test complete");
  await extension.unload();
  info("extension unloaded successfully");
});
