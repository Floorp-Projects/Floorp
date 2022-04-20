/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BuiltInThemeConfig } = ChromeUtils.import(
  "resource:///modules/BuiltInThemeConfig.jsm"
);

add_task(async function test_importConfig() {
  Assert.ok(
    BuiltInThemeConfig,
    "Was able to import BuiltInThemeConfig and it is not empty."
  );
});

add_task(function test_recoverEmptyHomepage() {
  for (let [id, config] of BuiltInThemeConfig.entries()) {
    if (id.endsWith("-colorway@mozilla.org")) {
      Assert.ok(config.collection, "Colorway theme has collection property.");
      Assert.ok(config.expiry, "Colorway theme has expiry date.");
    }
  }
});
