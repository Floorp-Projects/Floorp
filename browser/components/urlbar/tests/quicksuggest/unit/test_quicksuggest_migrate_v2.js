/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests quick suggest prefs migration to version 2.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
});

// Expected version 2 default-branch prefs
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
    "suggest.quicksuggest.nonsponsored": true,
    "suggest.quicksuggest.sponsored": true,
  },
};

// Migration will use these values to migrate only up to version 1 instead of
// the current version.
// Currently undefined because version 2 is the current migration version and we
// want migration to use its actual values, not overrides. When version 3 is
// added, set this to an object like the one in test_quicksuggest_migrate_v1.js.
const TEST_OVERRIDES = undefined;

add_setup(async () => {
  await UrlbarTestUtils.initNimbusFeature();
});

// The following tasks test OFFLINE UNVERSIONED to OFFLINE

// Migrating from:
// * Unversioned prefs
// * Offline
// * Main suggestions pref: user left on
// * Sponsored suggestions: user left on
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: on
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
// * Unversioned prefs
// * Offline
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
// * Unversioned prefs
// * Offline
// * Main suggestions pref: user left on
// * Sponsored suggestions: user turned off
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: off
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
// * Unversioned prefs
// * Offline
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

// The following tasks test OFFLINE UNVERSIONED to ONLINE

// Migrating from:
// * Unversioned prefs
// * Offline
// * Main suggestions pref: user left on
// * Sponsored suggestions: user left on
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: on
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
// * Unversioned prefs
// * Offline
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
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Migrating from:
// * Unversioned prefs
// * Offline
// * Main suggestions pref: user left on
// * Sponsored suggestions: user turned off
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
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
// * Unversioned prefs
// * Offline
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

// The following tasks test ONLINE UNVERSIONED to ONLINE when the user was NOT
// SHOWN THE MODAL (e.g., because they didn't restart)

// Migrating from:
// * Unversioned prefs
// * Online
// * Modal shown: no
// * User enrolled in online where suggestions were disabled by default, did not
//   turn on either type of suggestion, was not shown the modal (e.g., because
//   they didn't restart), and upgraded to v2
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function () {
  await withOnlineExperiment(async () => {
    await doMigrateTest({
      testOverrides: TEST_OVERRIDES,
      scenario: "online",
      expectedPrefs: {
        defaultBranch: DEFAULT_PREFS.online,
      },
    });
  });
});

// Migrating from:
// * Unversioned prefs
// * Online
// * Modal shown: no
// * User enrolled in online where suggestions were disabled by default, turned
//   on main suggestions pref and left off sponsored suggestions, was not shown
//   the modal (e.g., because they didn't restart), and upgraded to v2
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: off
// * Data collection: on (since they opted in by checking the main checkbox
//   while in online)
add_task(async function () {
  await withOnlineExperiment(async () => {
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
        },
      },
    });
  });
});

// Migrating from:
// * Unversioned prefs
// * Online
// * Modal shown: no
// * User enrolled in online where suggestions were disabled by default, left
//   off main suggestions pref and turned on sponsored suggestions, was not
//   shown the modal (e.g., because they didn't restart), and upgraded to v2
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function () {
  await withOnlineExperiment(async () => {
    await doMigrateTest({
      testOverrides: TEST_OVERRIDES,
      initialUserBranch: {
        "suggest.quicksuggest.sponsored": true,
      },
      scenario: "online",
      expectedPrefs: {
        defaultBranch: DEFAULT_PREFS.online,
        userBranch: {
          "suggest.quicksuggest.sponsored": true,
        },
      },
    });
  });
});

// Migrating from:
// * Unversioned prefs
// * Online
// * Modal shown: no
// * User enrolled in online where suggestions were disabled by default, turned
//   on main suggestions pref and sponsored suggestions, was not shown the
//   modal (e.g., because they didn't restart), and upgraded to v2
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: on
// * Data collection: on (since they opted in by checking the main checkbox
//   while in online)
add_task(async function () {
  await withOnlineExperiment(async () => {
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
        },
      },
    });
  });
});

// The following tasks test ONLINE UNVERSIONED to ONLINE when the user WAS SHOWN
// THE MODAL

// Migrating from:
// * Unversioned prefs
// * Online
// * Modal shown: yes
// * The following end up with same prefs and are covered by this task:
//   1. User did not opt in and left off both the main suggestions pref and
//      sponsored suggestions
//   2. User opted in but then later turned off both the main suggestions pref
//      and sponsored suggestions
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
      "quicksuggest.showedOnboardingDialog": true,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": false,
      },
    },
  });
});

// Migrating from:
// * Unversioned prefs
// * Online
// * Modal shown: yes
// * The following end up with same prefs and are covered by this task:
//   1. User did not opt in but then later turned on the main suggestions pref
//   2. User opted in but then later turned off sponsored suggestions
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: off
// * Data collection: on
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.showedOnboardingDialog": true,
      "suggest.quicksuggest": true,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Migrating from:
// * Unversioned prefs
// * Online
// * Modal shown: yes
// * The following end up with same prefs and are covered by this task:
//   1. User did not opt in but then later turned on sponsored suggestions
//   2. User opted in but then later turned off the main suggestions pref
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
      "quicksuggest.showedOnboardingDialog": true,
      "suggest.quicksuggest.sponsored": true,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": false,
      },
    },
  });
});

// Migrating from:
// * Unversioned prefs
// * Online
// * Modal shown: yes
// * The following end up with same prefs and are covered by this task:
//   1. User did not opt in but then later turned on both the main suggestions
//      pref and sponsored suggestions
//   2. User opted in and left on both the main suggestions pref and sponsored
//      suggestions
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: on
// * Data collection: on
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.showedOnboardingDialog": true,
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

// The following tasks test OFFLINE VERSION 1 to OFFLINE

// Migrating from:
// * Version 1 prefs
// * Offline
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user left on
// * Data collection: user left off
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: on
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "offline",
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
    },
  });
});

// Migrating from:
// * Version 1 prefs
// * Offline
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user left on
// * Data collection: user turned on
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: on
// * Data collection: on
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "offline",
      "quicksuggest.dataCollection.enabled": true,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
      userBranch: {
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Migrating from:
// * Version 1 prefs
// * Offline
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user turned off
// * Data collection: user left off
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "offline",
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
// * Version 1 prefs
// * Offline
// * Non-sponsored suggestions: user turned off
// * Sponsored suggestions: user turned off
// * Data collection: user left off
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
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "offline",
      "suggest.quicksuggest.nonsponsored": false,
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

// The following tasks test OFFLINE VERSION 1 to ONLINE

// Migrating from:
// * Version 1 prefs
// * Offline
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user left on
// * Data collection: user left off
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: on
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "offline",
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
    },
  });
});

// Migrating from:
// * Version 1 prefs
// * Offline
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user turned off
// * Data collection: user left off
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "offline",
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
// * Version 1 prefs
// * Offline
// * Non-sponsored suggestions: user turned off
// * Sponsored suggestions: user turned off
// * Data collection: user left off
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "offline",
      "suggest.quicksuggest.nonsponsored": false,
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

// The following tasks test ONLINE VERSION 1 to ONLINE when the user was NOT
// SHOWN THE MODAL (e.g., because they didn't restart)

// Migrating from:
// * Version 1 prefs
// * Online
// * Modal shown: no
// * Non-sponsored suggestions: user left off
// * Sponsored suggestions: user left off
// * Data collection: user left off
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
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "online",
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

// Migrating from:
// * Version 1 prefs
// * Online
// * Modal shown: no
// * Non-sponsored suggestions: user left off
// * Sponsored suggestions: user left off
// * Data collection: user turned on
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: off
// * Data collection: on
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "online",
      "quicksuggest.dataCollection.enabled": true,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Migrating from:
// * Version 1 prefs
// * Online
// * Modal shown: no
// * Non-sponsored suggestions: user left off
// * Sponsored suggestions: user turned on
// * Data collection: user left off
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: on
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "online",
      "suggest.quicksuggest.sponsored": true,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": true,
      },
    },
  });
});

// Migrating from:
// * Version 1 prefs
// * Online
// * Modal shown: no
// * Non-sponsored suggestions: user left off
// * Sponsored suggestions: user turned on
// * Data collection: user turned on
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: on
// * Data collection: on
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "online",
      "suggest.quicksuggest.sponsored": true,
      "quicksuggest.dataCollection.enabled": true,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": true,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Migrating from:
// * Version 1 prefs
// * Online
// * Modal shown: no
// * Non-sponsored suggestions: user turned on
// * Sponsored suggestions: user left off
// * Data collection: user left off
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "online",
      "suggest.quicksuggest.nonsponsored": true,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Migrating from:
// * Version 1 prefs
// * Online
// * Modal shown: no
// * Non-sponsored suggestions: user turned on
// * Sponsored suggestions: user left off
// * Data collection: user turned on
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: off
// * Data collection: on
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "online",
      "suggest.quicksuggest.nonsponsored": true,
      "quicksuggest.dataCollection.enabled": true,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Migrating from:
// * Version 1 prefs
// * Online
// * Modal shown: no
// * Non-sponsored suggestions: user turned on
// * Sponsored suggestions: user turned on
// * Data collection: user left off
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: on
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "online",
      "suggest.quicksuggest.nonsponsored": true,
      "suggest.quicksuggest.sponsored": true,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": true,
      },
    },
  });
});

// Migrating from:
// * Version 1 prefs
// * Online
// * Modal shown: no
// * Non-sponsored suggestions: user turned on
// * Sponsored suggestions: user turned on
// * Data collection: user turned on
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: on
// * Data collection: on
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "online",
      "suggest.quicksuggest.nonsponsored": true,
      "suggest.quicksuggest.sponsored": true,
      "quicksuggest.dataCollection.enabled": true,
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

// The following tasks test ONLINE VERSION 1 to ONLINE when the user WAS SHOWN
// THE MODAL WHILE PREFS WERE UNVERSIONED

// Migrating from:
// * Version 1 prefs
// * Online
// * Modal shown: yes, while prefs were unversioned
// * User opted in: no
// * Non-sponsored suggestions: user left off
// * Sponsored suggestions: user left off
// * Data collection: user left off
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
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "online",
      "quicksuggest.showedOnboardingDialog": true,
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

// Migrating from:
// * Version 1 prefs
// * Online
// * Modal shown: yes, while prefs were unversioned
// * User opted in: no
// * Non-sponsored suggestions: user turned on
// * Sponsored suggestions: user left off
// * Data collection: user left off
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "online",
      "quicksuggest.showedOnboardingDialog": true,
      "suggest.quicksuggest.nonsponsored": true,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Migrating from:
// * Version 1 prefs
// * Online
// * Modal shown: yes, while prefs were unversioned
// * User opted in: yes
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user left on
// * Data collection: user left on
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: on
// * Data collection: on
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "online",
      "quicksuggest.showedOnboardingDialog": true,
      "suggest.quicksuggest.nonsponsored": true,
      "suggest.quicksuggest.sponsored": true,
      "quicksuggest.dataCollection.enabled": true,
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

// Migrating from:
// * Version 1 prefs
// * Online
// * Modal shown: yes, while prefs were unversioned
// * User opted in: yes
// * Non-sponsored suggestions: user turned off
// * Sponsored suggestions: user left on
// * Data collection: user left on
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: on
// * Data collection: on
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "online",
      "quicksuggest.showedOnboardingDialog": true,
      "suggest.quicksuggest.sponsored": true,
      "quicksuggest.dataCollection.enabled": true,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": true,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Migrating from:
// * Version 1 prefs
// * Online
// * Modal shown: yes, while prefs were unversioned
// * User opted in: yes
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user left on
// * Data collection: user turned off
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: on
// * Data collection: on
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "online",
      "quicksuggest.showedOnboardingDialog": true,
      "suggest.quicksuggest.nonsponsored": true,
      "suggest.quicksuggest.sponsored": true,
      "quicksuggest.dataCollection.enabled": false,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": true,
        "quicksuggest.dataCollection.enabled": false,
      },
    },
  });
});

// The following tasks test ONLINE VERSION 1 to ONLINE when the user WAS SHOWN
// THE MODAL WHILE PREFS WERE VERSION 1

// Migrating from:
// * Version 1 prefs
// * Online
// * Modal shown: yes, while prefs were version 1
// * User opted in: no
// * Non-sponsored suggestions: user left off
// * Sponsored suggestions: user left off
// * Data collection: user left off
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
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "online",
      "quicksuggest.showedOnboardingDialog": true,
      "quicksuggest.onboardingDialogChoice": "not_now_link",
      "suggest.quicksuggest.nonsponsored": false,
      "suggest.quicksuggest.sponsored": false,
      "quicksuggest.dataCollection.enabled": false,
    },
    scenario: "online",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": false,
      },
    },
  });
});

// Migrating from:
// * Version 1 prefs
// * Online
// * Modal shown: yes, while prefs were version 1
// * User opted in: yes
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user left on
// * Data collection: user left on
//
// Scenario when migration occurs:
// * Online
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: on
// * Data collection: on
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "online",
      "quicksuggest.showedOnboardingDialog": true,
      "quicksuggest.onboardingDialogChoice": "accept",
      "suggest.quicksuggest.nonsponsored": true,
      "suggest.quicksuggest.sponsored": true,
      "quicksuggest.dataCollection.enabled": true,
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

async function withOnlineExperiment(callback) {
  let { enrollmentPromise, doExperimentCleanup } =
    ExperimentFakes.enrollmentHelper(
      ExperimentFakes.recipe("firefox-suggest-offline-vs-online", {
        active: true,
        branches: [
          {
            slug: "treatment",
            features: [
              {
                featureId: NimbusFeatures.urlbar.featureId,
                value: {
                  enabled: true,
                },
              },
            ],
          },
        ],
      })
    );
  await enrollmentPromise;
  await callback();
  await doExperimentCleanup();
}
