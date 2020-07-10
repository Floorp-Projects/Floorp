/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionStorageIDB",
  "resource://gre/modules/ExtensionStorageIDB.jsm"
);

const ADDON_ID = "doh-rollout@mozilla.org";

add_task(setup);

add_task(async function testLocalStorageMigration() {
  Preferences.reset(prefs.BALROG_MIGRATION_PREF);

  const legacyEntries = {
    doneFirstRun: true,
    "doh-rollout.doorhanger-decision": "UIOk",
    "doh-rollout.disable-heuristics": true,
  };

  let policy = WebExtensionPolicy.getByID(ADDON_ID);

  const storagePrincipal = ExtensionStorageIDB.getStoragePrincipal(
    policy.extension
  );

  const idbConn = await ExtensionStorageIDB.open(storagePrincipal);
  await idbConn.set(legacyEntries);

  let migrationDone = new Promise(resolve => {
    Preferences.observe(prefs.BALROG_MIGRATION_PREF, function obs() {
      Preferences.ignore(prefs.BALROG_MIGRATION_PREF, obs);
      resolve();
    });
  });

  await restartDoHController();
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

  await idbConn.clear();
  await idbConn.close();
});
