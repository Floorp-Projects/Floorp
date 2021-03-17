/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_remove_unsupported() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.userContext.enabled", true]],
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["browsingData"],
    },
    async background() {
      for (let dataType of [
        "cache",
        "downloads",
        "formData",
        "history",
        "passwords",
        "pluginData",
        "serviceWorkers",
      ]) {
        await browser.test.assertRejects(
          browser.browsingData.remove(
            { cookieStoreId: "firefox-default" },
            {
              [dataType]: true,
            }
          ),
          `Firefox does not support clearing ${dataType} with 'cookieStoreId'.`,
          `Should reject for unsupported dataType: ${dataType}`
        );
      }

      // Smoke test that doesn't delete anything.
      await browser.browsingData.remove(
        { cookieStoreId: "firefox-container-1" },
        {}
      );

      browser.test.sendMessage("done");
    },
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_invalid_id() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.userContext.enabled", true]],
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["browsingData"],
    },
    async background() {
      for (let cookieStoreId of [
        "firefox-DEFAULT", // should be "firefox-default"
        "firefox-private222",
        "firefox",
        "firefox-container-",
        "firefox-container-100000",
      ]) {
        await browser.test.assertRejects(
          browser.browsingData.remove({ cookieStoreId }, { cookies: true }),
          `Invalid cookieStoreId: ${cookieStoreId}`,
          `Should reject invalid cookieStoreId: ${cookieStoreId}`
        );
      }

      browser.test.sendMessage("done");
    },
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();

  await SpecialPowers.popPrefEnv();
});
