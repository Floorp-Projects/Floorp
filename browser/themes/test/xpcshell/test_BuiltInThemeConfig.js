/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { _applyColorwayConfig, BuiltInThemeConfig } = ChromeUtils.import(
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

add_task(function test_findActiveColorwayCollection() {
  const collections = [
    {
      id: 1,
      expiry: new Date("2012-01-01"),
    },
    {
      id: 2,
      expiry: new Date("2012-02-01"),
    },
    {
      id: 3,
      expiry: new Date("2012-03-01"),
    },
    {
      id: 4,
      expiry: new Date("2012-04-01"),
    },
    {
      id: 5,
      expiry: new Date("2012-04-02"),
    },
  ];
  _applyColorwayConfig(collections);
  Assert.ok(
    BuiltInThemeConfig.findActiveColorwayCollection(new Date("2010-01-01")),
    "Found active collection"
  );
  Assert.equal(
    BuiltInThemeConfig.findActiveColorwayCollection(new Date("2015-01-01")),
    null,
    "Found no active collection"
  );
  Assert.equal(
    BuiltInThemeConfig.findActiveColorwayCollection(new Date("2011-12-31")).id,
    1,
    "First collection is active"
  );
  Assert.equal(
    BuiltInThemeConfig.findActiveColorwayCollection(new Date("2012-02-03")).id,
    3,
    "Middle collection is active"
  );
  Assert.equal(
    BuiltInThemeConfig.findActiveColorwayCollection(new Date("2012-04-02")).id,
    5,
    "Last collection is active"
  );
  Assert.equal(
    BuiltInThemeConfig.findActiveColorwayCollection(new Date("2012-01-02")).id,
    2,
    "Second collection is active"
  );
});
