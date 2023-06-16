/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { EdgeProfileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/EdgeProfileMigrator.sys.mjs"
);
const { MSMigrationUtils } = ChromeUtils.importESModule(
  "resource:///modules/MSMigrationUtils.sys.mjs"
);

/**
 * Tests that history visits loaded from the registry from Edge (EdgeHTML)
 * that have a visit date older than maxAgeInDays days do not get imported.
 */
add_task(async function test_Edge_history_past_max_days() {
  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  Assert.ok(
    MigrationUtils.HISTORY_MAX_AGE_IN_DAYS < 300,
    "This test expects the current pref to be less than the youngest expired visit."
  );
  Assert.ok(
    MigrationUtils.HISTORY_MAX_AGE_IN_DAYS > 160,
    "This test expects the current pref to be greater than the oldest unexpired visit."
  );

  const EXPIRED_VISITS = [
    ["https://test1.invalid/", dateDaysAgo(500).getTime() * 1000],
    ["https://test2.invalid/", dateDaysAgo(450).getTime() * 1000],
    ["https://test3.invalid/", dateDaysAgo(300).getTime() * 1000],
  ];

  const UNEXPIRED_VISITS = [
    ["https://test4.invalid/"],
    ["https://test5.invalid/", dateDaysAgo(160).getTime() * 1000],
    ["https://test6.invalid/", dateDaysAgo(50).getTime() * 1000],
    ["https://test7.invalid/", dateDaysAgo(0).getTime() * 1000],
  ];

  const ALL_VISITS = [...EXPIRED_VISITS, ...UNEXPIRED_VISITS];

  // Fake out the getResources method of the migrator so that we return
  // a single fake MigratorResource per availableResourceType.
  sandbox.stub(MSMigrationUtils, "getTypedURLs").callsFake(() => {
    return new Map(ALL_VISITS);
  });

  // Manually create an EdgeProfileMigrator rather than going through
  // MigrationUtils.getMigrator to avoid the user data availability check, since
  // we're mocking out that stuff.
  let migrator = new EdgeProfileMigrator();
  let registryTypedHistoryMigrator =
    migrator.getHistoryRegistryMigratorForTesting();
  await new Promise(resolve => {
    registryTypedHistoryMigrator.migrate(resolve);
  });
  Assert.ok(true, "History from registry migration done!");

  for (let expiredEntry of EXPIRED_VISITS) {
    let entry = await PlacesUtils.history.fetch(expiredEntry[0], {
      includeVisits: true,
    });
    Assert.equal(entry, null, "Should not have found an entry.");
  }

  for (let unexpiredEntry of UNEXPIRED_VISITS) {
    let entry = await PlacesUtils.history.fetch(unexpiredEntry[0], {
      includeVisits: true,
    });
    Assert.equal(entry.url, unexpiredEntry[0], "Should have the correct URL");
    Assert.ok(!!entry.visits.length, "Should have some visits");
  }
});
