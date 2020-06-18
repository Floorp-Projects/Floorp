/**
 * Older versions of the doh-rollout add-on used local storage for memory.
 * The current version uses prefs, and migrates the old local storage values
 * to prefs. This tests the migration code.
 */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// Does some useful imports that set up message managers.
ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm",
  null
);

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionParent",
  "resource://gre/modules/ExtensionParent.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionStorageIDB",
  "resource://gre/modules/ExtensionStorageIDB.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);

const MIGRATION_DONE_PREF = "doh-rollout.balrog-migration-done";
const ADDON_ID = "doh-rollout@mozilla.org";

add_task(async function setup() {
  AddonTestUtils.createAppInfo(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "1",
    "72"
  );
  await AddonTestUtils.promiseStartupManager();

  let extensionPath = Services.dirsvc.get("GreD", Ci.nsIFile);
  extensionPath.append("browser");
  extensionPath.append("features");
  extensionPath.append(ADDON_ID);

  if (!extensionPath.exists()) {
    extensionPath.leafName = `${ADDON_ID}.xpi`;
  }

  let startupPromise = new Promise(resolve => {
    const { apiManager } = ExtensionParent;
    function onReady(event, extension) {
      if (extension.id == ADDON_ID) {
        apiManager.off("ready", onReady);
        resolve();
      }
    }

    apiManager.on("ready", onReady);
  });

  await AddonManager.installTemporaryAddon(extensionPath);
  await startupPromise;
});

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

    Assert.equal(
      Preferences.get(key),
      value,
      `${key} pref exists and has the right value ${value}`
    );

    Preferences.reset(key);
  }
});

add_task(async function testNextDNSMigration() {
  let oldURL = "https://trr.dns.nextdns.io/";
  let newURL = "https://firefox.dns.nextdns.io/";

  let prefChangePromises = [];
  let prefsToMigrate = {
    "network.trr.resolvers": `[{ "name": "Other Provider", "url": "https://sometrr.com/query" }, { "name": "NextDNS", "url": "${oldURL}" }]`,
    "network.trr.uri": oldURL,
    "network.trr.custom_uri": oldURL,
    "doh-rollout.trr-selection.dry-run-result": oldURL,
    "doh-rollout.uri": oldURL,
  };

  for (let [pref, value] of Object.entries(prefsToMigrate)) {
    Preferences.set(pref, value);

    prefChangePromises.push(
      new Promise(resolve => {
        Preferences.observe(pref, function obs() {
          Preferences.ignore(pref, obs);
          resolve();
        });
      })
    );
  }

  let migrationDone = Promise.all(prefChangePromises);
  let addon = await AddonManager.getAddonByID(ADDON_ID);
  await addon.reload();
  await migrationDone;

  for (let [pref, value] of Object.entries(prefsToMigrate)) {
    Assert.equal(
      Preferences.get(pref),
      value.replaceAll(oldURL, newURL),
      "Pref correctly migrated"
    );
  }
});
