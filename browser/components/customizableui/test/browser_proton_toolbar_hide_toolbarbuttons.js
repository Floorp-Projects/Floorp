/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "HomePage",
  "resource:///modules/HomePage.jsm"
);

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
  let oldState = CustomizableUI.getTestOnlyInternalProp("gSavedState");

  Assert.equal(
    Services.prefs.getIntPref(kPrefProtonToolbarVersion),
    0,
    "Toolbar proton version is 0"
  );

  let CustomizableUIInternal = CustomizableUI.getTestOnlyInternalProp(
    "CustomizableUIInternal"
  );

  CustomizableUI.setTestOnlyInternalProp("gSavedState", {
    placements: {
      "nav-bar": defaultPlacements,
    },
  });
  CustomizableUIInternal._updateForNewProtonVersion();

  let navbarPlacements = CustomizableUI.getTestOnlyInternalProp("gSavedState")
    .placements["nav-bar"];
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
  CustomizableUI.setTestOnlyInternalProp("gSavedState", oldState);
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
      prefs: [],
      actions: {
        shouldRemoveHomeButton: true,
        shouldRemoveLibraryButton: true,
        shouldRemoveSidebarButton: true,
        shouldUpdateVersion: true,
      },
    },
    // Proton enabled with home engagement
    {
      prefs: [[kPrefHomeButtonUsed, true]],
      actions: {
        shouldRemoveHomeButton: false,
        shouldRemoveLibraryButton: true,
        shouldRemoveSidebarButton: true,
        shouldUpdateVersion: true,
      },
    },
    // Proton enabled with custom homepage
    {
      prefs: [],
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
      prefs: [[kPrefLibraryButtonUsed, true]],
      actions: {
        shouldRemoveHomeButton: true,
        shouldRemoveLibraryButton: false,
        shouldRemoveSidebarButton: true,
        shouldUpdateVersion: true,
      },
    },
    // Proton enabled with sidebar engagement
    {
      prefs: [[kPrefSidebarButtonUsed, true]],
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
    set: [[kPrefProtonToolbarVersion, 0]],
  });
  let oldState = CustomizableUI.getTestOnlyInternalProp("gSavedState");

  Assert.equal(
    Services.prefs.getIntPref(kPrefProtonToolbarVersion),
    0,
    "Toolbar proton version is 0"
  );

  let CustomizableUIInternal = CustomizableUI.getTestOnlyInternalProp(
    "CustomizableUIInternal"
  );
  CustomizableUIInternal.initialize();

  Assert.ok(
    Services.prefs.getIntPref(kPrefProtonToolbarVersion) >= 1,
    "Toolbar proton version updated"
  );
  let navbarPlacements = CustomizableUI.getTestOnlyInternalProp("gAreas")
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
  CustomizableUI.setTestOnlyInternalProp("gSavedState", oldState);
  await SpecialPowers.popPrefEnv();
  // Re-initialize to prevent future test failures
  CustomizableUIInternal.initialize();
});

/**
 * Checks that a saved state that is missing nav-bar placements does not prevent migration.
 */
add_task(async function testNoNavbarPlacements() {
  await SpecialPowers.pushPrefEnv({
    set: [[kPrefProtonToolbarVersion, 0]],
  });

  let oldState = CustomizableUI.getTestOnlyInternalProp("gSavedState");

  Assert.equal(
    Services.prefs.getIntPref(kPrefProtonToolbarVersion),
    0,
    "Toolbar proton version is 0"
  );

  let CustomizableUIInternal = CustomizableUI.getTestOnlyInternalProp(
    "CustomizableUIInternal"
  );

  CustomizableUI.setTestOnlyInternalProp("gSavedState", {
    placements: { "widget-overflow-fixed-list": [] },
  });
  CustomizableUIInternal._updateForNewProtonVersion();

  Assert.ok(true, "_updateForNewProtonVersion didn't throw");

  // Cleanup
  CustomizableUI.setTestOnlyInternalProp("gSavedState", oldState);

  await SpecialPowers.popPrefEnv();
});

/**
 * Checks that a saved state that is missing the placements value does not prevent migration.
 */
add_task(async function testNullPlacements() {
  await SpecialPowers.pushPrefEnv({
    set: [[kPrefProtonToolbarVersion, 0]],
  });

  let oldState = CustomizableUI.getTestOnlyInternalProp("gSavedState");

  Assert.equal(
    Services.prefs.getIntPref(kPrefProtonToolbarVersion),
    0,
    "Toolbar proton version is 0"
  );

  let CustomizableUIInternal = CustomizableUI.getTestOnlyInternalProp(
    "CustomizableUIInternal"
  );

  CustomizableUI.setTestOnlyInternalProp("gSavedState", {});
  CustomizableUIInternal._updateForNewProtonVersion();

  Assert.ok(true, "_updateForNewProtonVersion didn't throw");

  // Cleanup
  CustomizableUI.setTestOnlyInternalProp("gSavedState", oldState);

  await SpecialPowers.popPrefEnv();
});
