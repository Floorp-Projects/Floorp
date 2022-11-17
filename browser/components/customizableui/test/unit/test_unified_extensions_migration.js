/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CustomizableUI } = ChromeUtils.import(
  "resource:///modules/CustomizableUI.jsm"
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

/**
 * Write customization state into CustomizableUI.
 *
 * @param {object} stateObj An object that will be structure-cloned and written
 *   into CustomizableUI's internal `gSavedState` state variable.
 */
function saveState(stateObj) {
  // We make sure to use structuredClone here so that we don't end up comparing
  // SAVED_STATE against itself.
  CustomizableUI.setTestOnlyInternalProp(
    "gSavedState",
    structuredClone(stateObj)
  );
}

/**
 * Read customization state out from CustomizableUI.
 *
 * @returns {object}
 */
function loadState() {
  return CustomizableUI.getTestOnlyInternalProp("gSavedState");
}

/**
 * Enables the Unified Extensions UI pref and calls the function to perform
 * forward migration.
 */
function migrateForward() {
  Services.prefs.setBoolPref("extensions.unifiedExtensions.enabled", true);
  CustomizableUIInternal._updateForUnifiedExtensions();
}

/**
 * Disables the Unified Extensions UI pref and calls the function to perform
 * backward migration.
 */
function migrateBackward() {
  Services.prefs.setBoolPref("extensions.unifiedExtensions.enabled", false);
  CustomizableUIInternal._updateForUnifiedExtensions();
}

/**
 * Test that attempting a migration on a new profile with no saved
 * state exits safely.
 */
add_task(async function test_no_saved_state() {
  saveState(null);
  migrateForward();
  Assert.equal(loadState(), null, "gSavedState should not have been modified");
});

/**
 * Test that attempting a migration on a new profile with no saved
 * state exits safely.
 */
add_task(async function test_no_saved_placements() {
  saveState({});
  migrateForward();
  Assert.deepEqual(
    loadState(),
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

  saveState(SAVED_STATE);
  migrateForward();

  Assert.deepEqual(
    loadState(),
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

  saveState(SAVED_STATE);
  migrateForward();

  Assert.deepEqual(
    loadState(),
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

  saveState(SAVED_STATE);
  migrateForward();

  Assert.deepEqual(
    loadState(),
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

  saveState(SAVED_STATE);
  migrateForward();

  Assert.deepEqual(
    loadState(),
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

  saveState(SAVED_STATE);
  migrateForward();

  Assert.deepEqual(
    loadState(),
    EXPECTED_STATE,
    "The saved state should not have changed after migration."
  );
});

/**
 * Test that migrating back appends the items in the addons panel
 * into the overflow panel. This should remove the AREA_ADDONS
 * placements too.
 */
add_task(async function test_migrating_back_with_items() {
  const SAVED_STATE = {
    placements: {
      "nav-bar": [
        "back-button",
        "forward-button",
        "spring",
        "urlbar-container",
        "save-to-pocket-button",
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

  const EXPECTED_STATE = structuredClone(SAVED_STATE);
  EXPECTED_STATE.placements[CustomizableUI.AREA_FIXED_OVERFLOW_PANEL] = [
    "privatebrowsing-button",
    "panic-button",
    "ext0-browser-action",
    "ext1-browser-action",
  ];
  delete EXPECTED_STATE.placements[CustomizableUI.AREA_ADDONS];

  saveState(SAVED_STATE);

  migrateBackward();

  Assert.deepEqual(
    loadState(),
    EXPECTED_STATE,
    "Migrating backward should append the addons items to the overflow panel."
  );
});

/**
 * Test that migrating back when there are no items in the addons panel
 * should not change any other area.
 */
add_task(async function test_migrating_back_with_no_items() {
  const SAVED_STATE = {
    placements: {
      "nav-bar": [
        "back-button",
        "forward-button",
        "spring",
        "urlbar-container",
        "save-to-pocket-button",
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
      "unified-extensions-area": [],
    },
  };

  const EXPECTED_STATE = structuredClone(SAVED_STATE);
  delete EXPECTED_STATE.placements[CustomizableUI.AREA_ADDONS];

  saveState(SAVED_STATE);

  migrateBackward();

  Assert.deepEqual(
    loadState(),
    EXPECTED_STATE,
    "Migrating backward should append the addons items to the overflow panel."
  );
});
