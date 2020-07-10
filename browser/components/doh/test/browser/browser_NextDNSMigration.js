/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_task(setup);

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
  await restartDoHController();
  await migrationDone;

  for (let [pref, value] of Object.entries(prefsToMigrate)) {
    is(
      Preferences.get(pref),
      value.replaceAll(oldURL, newURL),
      "Pref correctly migrated"
    );
    Preferences.reset(pref);
  }
});
