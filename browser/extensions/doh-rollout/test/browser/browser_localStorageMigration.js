/**
 * Older versions of the doh-rollout add-on used local storage for memory.
 * The current version uses prefs, and migrates the old local storage values
 * to prefs. This tests the migration code.
 */

"use strict";

const { ExtensionStorageIDB } = ChromeUtils.import(
  "resource://gre/modules/ExtensionStorageIDB.jsm"
);

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);

const MIGRATION_DONE_PREF = "doh-rollout.balrog-migration-done";
const ADDON_ID = "doh-rollout@mozilla.org";

add_task(async function testLocalStorageMigration() {
  Preferences.reset(MIGRATION_DONE_PREF);

  const legacyEntries = {
    doneFirstRun: true,
    skipHeuristicsCheck: true,
    "doh-rollout.previous.trr.mode": 2,
    "doh-rollout.doorhanger-shown": true,
    "doh-rollout.doorhanger-decision": "UIOk",
    "doh-rollout.disable-heuristics": true,
  };

  let policy = WebExtensionPolicy.getByID(ADDON_ID);

  const storagePrincipal = ExtensionStorageIDB.getStoragePrincipal(
    policy.extension
  );

  const idbConn = await ExtensionStorageIDB.open(storagePrincipal);
  await idbConn.set(legacyEntries);
  await idbConn.close();

  let migrationDone = new Promise(resolve => {
    Preferences.observe(MIGRATION_DONE_PREF, function obs() {
      Preferences.ignore(MIGRATION_DONE_PREF, obs);
      resolve();
    });
  });

  let addon = await AddonManager.getAddonByID(ADDON_ID);
  await addon.reload();

  await migrationDone;

  for (let [key, value] of Object.entries(legacyEntries)) {
    if (!key.startsWith("doh-rollout")) {
      key = "doh-rollout." + key;
    }

    is(
      Preferences.get(key),
      value,
      `${key} pref exists and has the right value ${value}`
    );

    Preferences.reset(key);
  }
});
