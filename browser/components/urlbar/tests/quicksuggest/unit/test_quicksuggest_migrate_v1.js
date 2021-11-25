/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests quick suggest prefs migration from unversioned prefs to version 1.

"use strict";

let gDefaultBranch = Services.prefs.getDefaultBranch("browser.urlbar.");
let gUserBranch = Services.prefs.getBranch("browser.urlbar.");

add_task(async function init() {
  await QuickSuggestTestUtils.initNimbusFeature();
});

// Migrating from:
// * History (quick suggest feature disabled)
//
// Scenario when migration occurs:
// * History
//
// Expected:
// * All history prefs set on the default branch
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.history,
    },
    scenario: "history",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.history,
    },
  });
});

// Migrating from:
// * History (quick suggest feature disabled)
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * All offline prefs set on the default branch
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.history,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
    },
  });
});

// Migrating from:
// * History (quick suggest feature disabled)
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * All online prefs set on the default branch
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.history,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
    },
  });
});

// The following tasks test OFFLINE TO OFFLINE

// Migrating from:
// * Offline (suggestions on by default)
// * User did not override any defaults
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain on
// * Data collection: off
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
    },
  });
});

// Migrating from:
// * Offline (suggestions on by default)
// * Main suggestions pref: user left on
// * Sponsored suggestions: user turned off
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: remain off
// * Data collection: off
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
      },
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Migrating from:
// * Offline (suggestions on by default)
// * Main suggestions pref: user turned off
// * Sponsored suggestions: user left on (but ignored since main was off)
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest": false,
      },
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Migrating from:
// * Offline (suggestions on by default)
// * Main suggestions pref: user turned off
// * Sponsored suggestions: user turned off
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// The following tasks test OFFLINE TO ONLINE

// Migrating from:
// * Offline (suggestions on by default)
// * User did not override any defaults
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
    },
  });
});

// Migrating from:
// * Offline (suggestions on by default)
// * Main suggestions pref: user left on
// * Sponsored suggestions: user turned off
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
      },
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Migrating from:
// * Offline (suggestions on by default)
// * Main suggestions pref: user turned off
// * Sponsored suggestions: user left on (but ignored since main was off)
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest": false,
      },
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
      },
    },
  });
});

// Migrating from:
// * Offline (suggestions on by default)
// * Main suggestions pref: user turned off
// * Sponsored suggestions: user turned off
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// The following tasks test ONLINE TO OFFLINE

// Migrating from:
// * Online (suggestions off by default)
// * User did not override any defaults
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: on (since main pref had default value)
// * Sponsored suggestions: on (since main & sponsored prefs had default values)
// * Data collection: off
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
    },
  });
});

// Migrating from:
// * Online (suggestions off by default)
// * Main suggestions pref: user left off
// * Sponsored suggestions: user turned on (but ignored since main was off)
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: on (see below)
// * Data collection: off
//
// It's unfortunate that sponsored suggestions are ultimately on since before
// the migration no suggestions were shown to the user. There's nothing we can
// do about it, aside from forcing off suggestions in more cases than we want.
// The reason is that at the time of migration we can't tell that the previous
// scenario was online -- or more precisely that it wasn't history. If we knew
// it wasn't history, then we'd know to turn sponsored off; if we knew it *was*
// history, then we'd know to turn sponsored -- and non-sponsored -- on, since
// the scenario at the time of migration is offline, where suggestions should be
// enabled by default.
//
// This is the reason we now record `quicksuggest.scenario` on the user branch
// and not the default branch as we previously did.
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.sponsored": true,
      },
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.sponsored": true,
      },
    },
  });
});

// Migrating from:
// * Online (suggestions off by default)
// * Main suggestions pref: user turned on
// * Sponsored suggestions: user left off
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain off
// * Data collection: off (since scenario is offline)
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest": true,
      },
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": true,
      },
    },
  });
});

// Migrating from:
// * Online (suggestions off by default)
// * Main suggestions pref: user turned on
// * Sponsored suggestions: user turned on
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain on
// * Data collection: off (since scenario is offline)
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest": true,
        "suggest.quicksuggest.sponsored": true,
      },
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": true,
      },
    },
  });
});

// The following tasks test ONLINE TO ONLINE

// Migrating from:
// * Online (suggestions off by default)
// * User did not override any defaults
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
    },
  });
});

// Migrating from:
// * Online (suggestions off by default)
// * Main suggestions pref: user left off
// * Sponsored suggestions: user turned on (but ignored since main was off)
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain off
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.sponsored": true,
      },
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
    },
  });
});

// Migrating from:
// * Online (suggestions off by default)
// * Main suggestions pref: user turned on
// * Sponsored suggestions: user left off
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain off
// * Data collection: ON (since user effectively opted in by turning on
//   suggestions)
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest": true,
      },
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": true,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Migrating from:
// * Online (suggestions off by default)
// * Main suggestions pref: user turned on
// * Sponsored suggestions: user turned on
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain on
// * Data collection: ON (since user effectively opted in by turning on
//   suggestions)
add_task(async function() {
  await doMigrateTest({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest": true,
        "suggest.quicksuggest.sponsored": true,
      },
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": true,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

async function doMigrateTest({ initialPrefsToSet, scenario, expectedPrefs }) {
  info(
    "Testing: " + JSON.stringify({ initialPrefsToSet, scenario, expectedPrefs })
  );

  // Set initial prefs.
  UrlbarPrefs._updatingFirefoxSuggestScenario = true;
  let {
    defaultBranch: initialDefaultBranch,
    userBranch: initialUserBranch,
  } = initialPrefsToSet;
  initialDefaultBranch = initialDefaultBranch || {};
  initialUserBranch = initialUserBranch || {};
  for (let name of Object.keys(initialDefaultBranch)) {
    // Clear user-branch values on the default prefs so the defaults aren't
    // masked.
    gUserBranch.clearUserPref(name);
  }
  UrlbarPrefs.clear("quicksuggest.migrationVersion");
  for (let [branch, prefs] of [
    [gDefaultBranch, initialDefaultBranch],
    [gUserBranch, initialUserBranch],
  ]) {
    for (let [name, value] of Object.entries(prefs)) {
      branch.setBoolPref(name, value);
    }
  }
  UrlbarPrefs._updatingFirefoxSuggestScenario = false;

  // Update the scenario and check prefs twice. The first time the migration
  // should happen, and the second time the migration should not happen and all
  // the prefs should stay the same.
  for (let i = 0; i < 2; i++) {
    // Do the scenario update and pass true to simulate startup.
    await UrlbarPrefs.updateFirefoxSuggestScenario(true, scenario);

    // Check expected pref values. Store expected effective values as we go
    // so we can check them afterward. For a given pref, the expected
    // effective value is the user value, or if there's not a user value,
    // the default value.
    let expectedEffectivePrefs = {};
    let {
      defaultBranch: expectedDefaultBranch,
      userBranch: expectedUserBranch,
    } = expectedPrefs;
    expectedDefaultBranch = expectedDefaultBranch || {};
    expectedUserBranch = expectedUserBranch || {};
    for (let [branch, prefs, branchType] of [
      [gDefaultBranch, expectedDefaultBranch, "default"],
      [gUserBranch, expectedUserBranch, "user"],
    ]) {
      for (let [name, value] of Object.entries(prefs)) {
        expectedEffectivePrefs[name] = value;
        if (branch == gUserBranch) {
          Assert.ok(
            gUserBranch.prefHasUserValue(name),
            `Pref ${name} is on user branch`
          );
        }
        Assert.equal(
          branch.getBoolPref(name),
          value,
          `Pref ${name} on ${branchType} branch`
        );
      }
    }
    for (let name of Object.keys(initialDefaultBranch)) {
      if (!expectedUserBranch.hasOwnProperty(name)) {
        Assert.ok(
          !gUserBranch.prefHasUserValue(name),
          `Pref ${name} is not on user branch`
        );
      }
    }
    for (let [name, value] of Object.entries(expectedEffectivePrefs)) {
      Assert.equal(
        UrlbarPrefs.get(name),
        value,
        `Pref ${name} effective value`
      );
    }
  }

  // Clean up.
  UrlbarPrefs._updatingFirefoxSuggestScenario = true;
  UrlbarPrefs.clear("quicksuggest.migrationVersion");
  for (let name of Object.keys(expectedPrefs.userBranch || {})) {
    UrlbarPrefs.clear(name);
  }
  UrlbarPrefs._updatingFirefoxSuggestScenario = false;
}
