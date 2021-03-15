/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let CustomizableUIBSPass = ChromeUtils.import(
  "resource:///modules/CustomizableUI.jsm",
  null
);

ChromeUtils.defineModuleGetter(
  this,
  "HomePage",
  "resource:///modules/HomePage.jsm"
);

const kPrefProtonToolbarEnabled = "browser.proton.enabled";
const kPrefProtonToolbarVersion = "browser.proton.toolbar.version";
const kPrefHomeButtonUsed = "browser.engagement.home-button.has-used";
const kPrefLibraryButtonUsed = "browser.engagement.library-button.has-used";
const kPrefSidebarButtonUsed = "browser.engagement.sidebar-button.has-used";

async function testToolbarButtons(aActions) {
  let {
    shouldRemoveHomeButton,
    shouldRemoveLibraryButton,
    shouldRemoveSidebarButton,
    shouldUpdateVersion,
  } = aActions;
  const defaultPlacements = [
    "back-button",
    "forward-button",
    "stop-reload-button",
    "home-button",
    "customizableui-special-spring1",
    "urlbar-container",
    "customizableui-special-spring2",
    "downloads-button",
    "library-button",
    "sidebar-button",
    "fxa-toolbar-menu-button",
  ];
  let oldState = CustomizableUIBSPass.gSavedState;

  Assert.equal(
    Services.prefs.getIntPref(kPrefProtonToolbarVersion),
    0,
    "Toolbar proton version is 0"
  );

  let { CustomizableUIInternal } = CustomizableUIBSPass;

  CustomizableUIBSPass.gSavedState = {
    placements: {
      "nav-bar": defaultPlacements,
    },
  };
  CustomizableUIInternal._updateForNewProtonVersion();

  let navbarPlacements = CustomizableUIBSPass.gSavedState.placements["nav-bar"];
  let includesHomeButton = navbarPlacements.includes("home-button");
  let includesLibraryButton = navbarPlacements.includes("library-button");
  let includesSidebarButton = navbarPlacements.includes("sidebar-button");

  Assert.equal(
    !includesHomeButton,
    shouldRemoveHomeButton,
    "Correctly handles home button"
  );
  Assert.equal(
    !includesLibraryButton,
    shouldRemoveLibraryButton,
    "Correctly handles library button"
  );
  Assert.equal(
    !includesSidebarButton,
    shouldRemoveSidebarButton,
    "Correctly handles sidebar button"
  );

  let toolbarVersion = Services.prefs.getIntPref(kPrefProtonToolbarVersion);
  if (shouldUpdateVersion) {
    Assert.ok(toolbarVersion >= 1, "Toolbar proton version updated");
  } else {
    Assert.ok(toolbarVersion == 0, "Toolbar proton version not updated");
  }

  // Cleanup
  CustomizableUIBSPass.gSavedState = oldState;
}

/**
 * Checks that the home button is removed from the nav-bar under
 * these conditions: proton must be enabled, the toolbar engagement
 * pref is false, and the homepage is about:home or about:blank.
 * Otherwise, the home button should remain if it was previously
 * in the navbar.
 * Also checks that the library button is removed from the nav-bar
 * if proton is enabled and the toolbar engagement pref is false.
 */
add_task(async function testButtonRemoval() {
  // Ensure the engagement prefs are set to their default values
  await SpecialPowers.pushPrefEnv({
    set: [
      [kPrefHomeButtonUsed, false],
      [kPrefLibraryButtonUsed, false],
      [kPrefSidebarButtonUsed, false],
    ],
  });

  let tests = [
    // Proton enabled without home and library engagement
    {
      prefs: [[kPrefProtonToolbarEnabled, true]],
      actions: {
        shouldRemoveHomeButton: true,
        shouldRemoveLibraryButton: true,
        shouldRemoveSidebarButton: true,
        shouldUpdateVersion: true,
      },
    },
    // Proton enabled with home engagement
    {
      prefs: [
        [kPrefProtonToolbarEnabled, true],
        [kPrefHomeButtonUsed, true],
      ],
      actions: {
        shouldRemoveHomeButton: false,
        shouldRemoveLibraryButton: true,
        shouldRemoveSidebarButton: true,
        shouldUpdateVersion: true,
      },
    },
    // Proton disabled
    {
      prefs: [[kPrefProtonToolbarEnabled, false]],
      actions: {
        shouldRemoveHomeButton: false,
        shouldRemoveLibraryButton: false,
        shouldRemoveSidebarButton: false,
        shouldUpdateVersion: false,
      },
    },
    // Proton enabled with custom homepage
    {
      prefs: [[kPrefProtonToolbarEnabled, true]],
      actions: {
        shouldRemoveHomeButton: false,
        shouldRemoveLibraryButton: true,
        shouldRemoveSidebarButton: true,
        shouldUpdateVersion: true,
      },
      async fn() {
        HomePage.safeSet("https://example.com");
      },
    },
    // Proton enabled with library engagement
    {
      prefs: [
        [kPrefProtonToolbarEnabled, true],
        [kPrefLibraryButtonUsed, true],
      ],
      actions: {
        shouldRemoveHomeButton: true,
        shouldRemoveLibraryButton: false,
        shouldRemoveSidebarButton: true,
        shouldUpdateVersion: true,
      },
    },
    // Proton enabled with sidebar engagement
    {
      prefs: [
        [kPrefProtonToolbarEnabled, true],
        [kPrefSidebarButtonUsed, true],
      ],
      actions: {
        shouldRemoveHomeButton: true,
        shouldRemoveLibraryButton: true,
        shouldRemoveSidebarButton: false,
        shouldUpdateVersion: true,
      },
    },
  ];

  for (let test of tests) {
    await SpecialPowers.pushPrefEnv({
      set: [[kPrefProtonToolbarVersion, 0], ...test.prefs],
    });
    if (test.fn) {
      await test.fn();
    }
    testToolbarButtons(test.actions);
    HomePage.reset();
    await SpecialPowers.popPrefEnv();
  }
});

/**
 * Checks that a null saved state (new profile) does not prevent migration.
 */
add_task(async function testNullSavedState() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [kPrefProtonToolbarVersion, 0],
      [kPrefProtonToolbarEnabled, true],
    ],
  });
  let oldState = CustomizableUIBSPass.gSavedState;

  Assert.equal(
    Services.prefs.getIntPref(kPrefProtonToolbarVersion),
    0,
    "Toolbar proton version is 0"
  );

  let { CustomizableUIInternal } = CustomizableUIBSPass;
  CustomizableUIInternal.initialize();

  Assert.ok(
    Services.prefs.getIntPref(kPrefProtonToolbarVersion) >= 1,
    "Toolbar proton version updated"
  );
  let navbarPlacements = CustomizableUIBSPass.gAreas
    .get("nav-bar")
    .get("defaultPlacements");
  Assert.ok(
    !navbarPlacements.includes("home-button"),
    "Home button isn't included by default"
  );
  Assert.ok(
    !navbarPlacements.includes("library-button"),
    "Library button isn't included by default"
  );
  Assert.ok(
    !navbarPlacements.includes("sidebar-button"),
    "Sidebar button isn't included by default"
  );

  // Cleanup
  CustomizableUIBSPass.gSavedState = oldState;
  await SpecialPowers.popPrefEnv();
  // Re-initialize to prevent future test failures
  CustomizableUIInternal.initialize();
});
