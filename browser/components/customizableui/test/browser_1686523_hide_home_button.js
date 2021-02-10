/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-disable-next-line mozilla/reject-chromeutils-import-null */
let CustomizableUIBSPass = ChromeUtils.import(
  "resource:///modules/CustomizableUI.jsm",
  null
);

ChromeUtils.defineModuleGetter(
  this,
  "HomePage",
  "resource:///modules/HomePage.jsm"
);

const kPrefProtonToolbarEnabled = "browser.proton.toolbar.enabled";
const kPrefProtonToolbarVersion = "browser.proton.toolbar.version";
const kPrefHomeButtonUsed = "browser.engagement.home-button.has-used";

async function testHomeButton(shouldRemoveHomeButton, shouldUpdateVersion) {
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

  let includesHomeButton = CustomizableUIBSPass.gSavedState.placements[
    "nav-bar"
  ].includes("home-button");

  Assert.equal(
    !includesHomeButton,
    shouldRemoveHomeButton,
    "Correctly handles home button"
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
 */
add_task(async function() {
  let tests = [
    // Proton enabled without engagement
    {
      prefs: [[kPrefProtonToolbarEnabled, true]],
      shouldRemoveHomeButton: true,
      shouldUpdateVersion: true,
    },
    // Proton enabled with engagement
    {
      prefs: [
        [kPrefProtonToolbarEnabled, true],
        [kPrefHomeButtonUsed, true],
      ],
      shouldRemoveHomeButton: false,
      shouldUpdateVersion: true,
    },
    // Proton disabled
    {
      prefs: [[kPrefProtonToolbarEnabled, false]],
      shouldRemoveHomeButton: false,
      shouldUpdateVersion: false,
    },
    // Proton enabled with custom homepage
    {
      prefs: [[kPrefProtonToolbarEnabled, true]],
      shouldRemoveHomeButton: false,
      shouldUpdateVersion: true,
      async fn() {
        HomePage.safeSet("https://example.com");
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
    testHomeButton(test.shouldRemoveHomeButton, test.shouldUpdateVersion);
    HomePage.reset();
    await SpecialPowers.popPrefEnv();
  }
});
