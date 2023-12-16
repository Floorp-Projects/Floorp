/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests quick suggest prefs migration from unversioned prefs to version 1.

"use strict";

// Expected version 1 default-branch prefs
const DEFAULT_PREFS = {
  history: {
    "quicksuggest.enabled": false,
  },
  offline: {
    "quicksuggest.enabled": true,
    "quicksuggest.dataCollection.enabled": false,
    "quicksuggest.shouldShowOnboardingDialog": false,
    "suggest.quicksuggest.nonsponsored": true,
    "suggest.quicksuggest.sponsored": true,
  },
  online: {
    "quicksuggest.enabled": true,
    "quicksuggest.dataCollection.enabled": false,
    "quicksuggest.shouldShowOnboardingDialog": true,
    "suggest.quicksuggest.nonsponsored": false,
    "suggest.quicksuggest.sponsored": false,
  },
};

// Migration will use these values to migrate only up to version 1 instead of
// the current version.
const TEST_OVERRIDES = {
  migrationVersion: 1,
  defaultPrefs: DEFAULT_PREFS,
};

add_setup(async () => {
  await UrlbarTestUtils.initNimbusFeature();
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest.sponsored": false,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest": false,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest": false,
      "suggest.quicksuggest.sponsored": false,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest.sponsored": false,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest": false,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest": false,
      "suggest.quicksuggest.sponsored": false,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest.sponsored": true,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest": true,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest": true,
      "suggest.quicksuggest.sponsored": true,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest.sponsored": true,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest": true,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
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
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest": true,
      "suggest.quicksuggest.sponsored": true,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": true,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});
