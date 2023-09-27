/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// We're in an xpcshell test but have an eslint browser test env applied;
// We definitely do need to manually import CustomizableUI.
// eslint-disable-next-line mozilla/no-redeclare-with-import-autofix
const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs"
);

do_get_profile();

// Make Cu.isInAutomation true. This is necessary so that we can use
// CustomizableUIInternal.
Services.prefs.setBoolPref(
  "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer",
  true
);

const CustomizableUIInternal = CustomizableUI.getTestOnlyInternalProp(
  "CustomizableUIInternal"
);

// Migration 19 was the Unified Extensions migration version introduced
// in 109, so we'll run tests by artificially setting the migration version
// to one value earlier.
const PRIOR_MIGRATION_VERSION = 18;

/**
 * Writes customization state into CustomizableUI and then performs the forward migration
 * for Unified Extensions.
 *
 * @param {object|null} stateObj An object that will be structure-cloned and
 *   written into CustomizableUI's internal `gSavedState` state variable. Should
 *   not include the currentVersion property, as this will be set automatically by
 *   function if stateObj is not null.
 * @returns {object}
 *   the saved state object (minus the currentVersion property).
 */
function migrateForward(stateObj) {
  // We make sure to use structuredClone here so that we don't end up comparing
  // SAVED_STATE against itself.
  let stateToSave = structuredClone(stateObj);
  if (stateToSave) {
    stateToSave.currentVersion = PRIOR_MIGRATION_VERSION;
  }

  CustomizableUI.setTestOnlyInternalProp("gSavedState", stateToSave);
  CustomizableUIInternal._updateForNewVersion();

  let migratedState = CustomizableUI.getTestOnlyInternalProp("gSavedState");
  if (migratedState) {
    delete migratedState.currentVersion;
  }
  return migratedState;
}

/**
 * Test that attempting a migration on a new profile with no saved
 * state exits safely.
 */
add_task(async function test_no_saved_state() {
  let migratedState = migrateForward(null);

  Assert.deepEqual(
    migratedState,
    null,
    "gSavedState should not have been modified"
  );
});

/**
 * Test that attempting a migration on a new profile with no saved
 * state exits safely.
 */
add_task(async function test_no_saved_placements() {
  let migratedState = migrateForward({});

  Assert.deepEqual(
    migratedState,
    {},
    "gSavedState should not have been modified"
  );
});

/**
 * Test that placements that don't involve any extension buttons are
 * not changed during the migration.
 */
add_task(async function test_no_extensions() {
  const SAVED_STATE = {
    placements: {
      "nav-bar": [
        "back-button",
        "forward-button",
        "spring",
        "urlbar-container",
        "save-to-pocket-button",
        "reset-pbm-toolbar-button",
      ],
      "toolbar-menubar": [
        "home-button",
        "menubar-items",
        "spring",
        "downloads-button",
      ],
      TabsToolbar: [
        "firefox-view-button",
        "tabbrowser-tabs",
        "new-tab-button",
        "alltabs-button",
        "developer-button",
      ],
      PersonalToolbar: ["personal-bookmarks", "fxa-toolbar-menu-button"],
      "widget-overflow-fixed-list": ["privatebrowsing-button", "panic-button"],
    },
  };

  // ADDONS_AREA should end up with an empty array as its set of placements.
  const EXPECTED_STATE = structuredClone(SAVED_STATE);
  EXPECTED_STATE.placements[CustomizableUI.AREA_ADDONS] = [];

  let migratedState = migrateForward(SAVED_STATE);

  Assert.deepEqual(
    migratedState,
    EXPECTED_STATE,
    "Got the expected state after the migration."
  );
});

/**
 * Test that if there's an existing set of items in CustomizableUI.AREA_ADDONS,
 * and no extension buttons to migrate from the overflow menu, then we don't
 * change the state at all.
 */
add_task(async function test_existing_browser_actions_no_movement() {
  const SAVED_STATE = {
    placements: {
      "nav-bar": [
        "back-button",
        "forward-button",
        "spring",
        "urlbar-container",
        "save-to-pocket-button",
        "reset-pbm-toolbar-button",
      ],
      "toolbar-menubar": [
        "home-button",
        "menubar-items",
        "spring",
        "downloads-button",
      ],
      TabsToolbar: [
        "firefox-view-button",
        "tabbrowser-tabs",
        "new-tab-button",
        "alltabs-button",
        "developer-button",
      ],
      PersonalToolbar: ["personal-bookmarks", "fxa-toolbar-menu-button"],
      "widget-overflow-fixed-list": ["privatebrowsing-button", "panic-button"],
      "unified-extensions-area": ["ext0-browser-action", "ext1-browser-action"],
    },
  };

  let migratedState = migrateForward(SAVED_STATE);

  Assert.deepEqual(
    migratedState,
    SAVED_STATE,
    "The saved state should not have changed after migration."
  );
});

/**
 * Test that we can migrate extension buttons out from the overflow panel
 * into the addons panel.
 */
add_task(async function test_migrate_extension_buttons() {
  const SAVED_STATE = {
    placements: {
      "nav-bar": [
        "back-button",
        "forward-button",
        "spring",
        "urlbar-container",
        "save-to-pocket-button",
        "reset-pbm-toolbar-button",
      ],
      "toolbar-menubar": [
        "home-button",
        "menubar-items",
        "spring",
        "downloads-button",
      ],
      TabsToolbar: [
        "firefox-view-button",
        "tabbrowser-tabs",
        "new-tab-button",
        "alltabs-button",
        "developer-button",
      ],
      PersonalToolbar: ["personal-bookmarks", "fxa-toolbar-menu-button"],
      "widget-overflow-fixed-list": [
        "ext0-browser-action",
        "privatebrowsing-button",
        "ext1-browser-action",
        "panic-button",
        "ext2-browser-action",
      ],
    },
  };
  const EXPECTED_STATE = structuredClone(SAVED_STATE);
  EXPECTED_STATE.placements[CustomizableUI.AREA_FIXED_OVERFLOW_PANEL] = [
    "privatebrowsing-button",
    "panic-button",
  ];
  EXPECTED_STATE.placements[CustomizableUI.AREA_ADDONS] = [
    "ext0-browser-action",
    "ext1-browser-action",
    "ext2-browser-action",
  ];

  let migratedState = migrateForward(SAVED_STATE);

  Assert.deepEqual(
    migratedState,
    EXPECTED_STATE,
    "The saved state should not have changed after migration."
  );
});

/**
 * Test that we won't overwrite existing placements within the addons panel
 * if we migrate things over from the overflow panel. We'll prepend the
 * migrated items to the addons panel instead.
 */
add_task(async function test_migrate_extension_buttons_no_overwrite() {
  const SAVED_STATE = {
    placements: {
      "nav-bar": [
        "back-button",
        "forward-button",
        "spring",
        "urlbar-container",
        "save-to-pocket-button",
        "reset-pbm-toolbar-button",
      ],
      "toolbar-menubar": [
        "home-button",
        "menubar-items",
        "spring",
        "downloads-button",
      ],
      TabsToolbar: [
        "firefox-view-button",
        "tabbrowser-tabs",
        "new-tab-button",
        "alltabs-button",
        "developer-button",
      ],
      PersonalToolbar: ["personal-bookmarks", "fxa-toolbar-menu-button"],
      "widget-overflow-fixed-list": [
        "ext0-browser-action",
        "privatebrowsing-button",
        "ext1-browser-action",
        "panic-button",
        "ext2-browser-action",
      ],
      "unified-extensions-area": ["ext3-browser-action", "ext4-browser-action"],
    },
  };
  const EXPECTED_STATE = structuredClone(SAVED_STATE);
  EXPECTED_STATE.placements[CustomizableUI.AREA_FIXED_OVERFLOW_PANEL] = [
    "privatebrowsing-button",
    "panic-button",
  ];
  EXPECTED_STATE.placements[CustomizableUI.AREA_ADDONS] = [
    "ext0-browser-action",
    "ext1-browser-action",
    "ext2-browser-action",
    "ext3-browser-action",
    "ext4-browser-action",
  ];

  let migratedState = migrateForward(SAVED_STATE);

  Assert.deepEqual(
    migratedState,
    EXPECTED_STATE,
    "The saved state should not have changed after migration."
  );
});

/**
 * Test that extension buttons from areas other than the overflow panel
 * won't be moved.
 */
add_task(async function test_migrate_extension_buttons_elsewhere() {
  const SAVED_STATE = {
    placements: {
      "nav-bar": [
        "back-button",
        "ext0-browser-action",
        "forward-button",
        "ext1-browser-action",
        "spring",
        "ext2-browser-action",
        "urlbar-container",
        "ext3-browser-action",
        "save-to-pocket-button",
        "reset-pbm-toolbar-button",
        "ext4-browser-action",
      ],
      "toolbar-menubar": [
        "home-button",
        "ext5-browser-action",
        "menubar-items",
        "ext6-browser-action",
        "spring",
        "ext7-browser-action",
        "downloads-button",
        "ext8-browser-action",
      ],
      TabsToolbar: [
        "firefox-view-button",
        "ext9-browser-action",
        "tabbrowser-tabs",
        "ext10-browser-action",
        "new-tab-button",
        "ext11-browser-action",
        "alltabs-button",
        "ext12-browser-action",
        "developer-button",
        "ext13-browser-action",
      ],
      PersonalToolbar: [
        "personal-bookmarks",
        "ext14-browser-action",
        "fxa-toolbar-menu-button",
        "ext15-browser-action",
      ],
      "widget-overflow-fixed-list": [
        "ext16-browser-action",
        "privatebrowsing-button",
        "ext17-browser-action",
        "panic-button",
        "ext18-browser-action",
      ],
    },
  };
  const EXPECTED_STATE = structuredClone(SAVED_STATE);
  EXPECTED_STATE.placements[CustomizableUI.AREA_FIXED_OVERFLOW_PANEL] = [
    "privatebrowsing-button",
    "panic-button",
  ];
  EXPECTED_STATE.placements[CustomizableUI.AREA_ADDONS] = [
    "ext16-browser-action",
    "ext17-browser-action",
    "ext18-browser-action",
  ];

  let migratedState = migrateForward(SAVED_STATE);

  Assert.deepEqual(
    migratedState,
    EXPECTED_STATE,
    "The saved state should not have changed after migration."
  );
});
