/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testInvalidArguments() {
  async function background() {
    const UNSUPPORTED_DATA_TYPES = ["appcache", "fileSystems", "webSQL"];

    await browser.test.assertRejects(
      browser.browsingData.remove({originTypes: {protectedWeb: true}}, {cookies: true}),
      "Firefox does not support protectedWeb or extension as originTypes.",
      "Expected error received when using protectedWeb originType.");

    await browser.test.assertRejects(
      browser.browsingData.removeCookies({originTypes: {extension: true}}),
      "Firefox does not support protectedWeb or extension as originTypes.",
      "Expected error received when using extension originType.");

    for (let dataType of UNSUPPORTED_DATA_TYPES) {
      let dataTypes = {};
      dataTypes[dataType] = true;
      browser.test.assertThrows(
        () => browser.browsingData.remove({}, dataTypes),
        /Type error for parameter dataToRemove/,
        `Expected error received when using ${dataType} dataType.`
      );
    }

    browser.test.notifyPass("invalidArguments");
  }

  let extensionData = {
    background: background,
    manifest: {
      permissions: ["browsingData"],
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  await extension.awaitFinish("invalidArguments");
  await extension.unload();
});
