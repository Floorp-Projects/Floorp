/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CustomizableUIInternal = CustomizableUI.getTestOnlyInternalProp(
  "CustomizableUIInternal"
);
let gAreas = CustomizableUI.getTestOnlyInternalProp("gAreas");

const SIDEBAR_BUTTON_INTRODUCED_PREF =
  "browser.toolbarbuttons.introduced.sidebar-button";
const SIDEBAR_REVAMP_PREF = "sidebar.revamp";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      [SIDEBAR_REVAMP_PREF, true],
      [SIDEBAR_BUTTON_INTRODUCED_PREF, false],
    ],
  });
  let navbarDefaults = gAreas.get("nav-bar").get("defaultPlacements");
  let hadSavedState = !!CustomizableUI.getTestOnlyInternalProp("gSavedState");
  if (!hadSavedState) {
    CustomizableUI.setTestOnlyInternalProp("gSavedState", {
      currentVersion: CustomizableUI.getTestOnlyInternalProp("kVersion"),
      placements: {
        "nav-bar": Array.from(navbarDefaults),
      },
    });
  }

  // Normally, if the `sidebar.revamp` pref is set at startup,
  // we'll have it in the default placements for the navbar.
  // Simulate this from the test:
  let backupDefaults = Array.from(navbarDefaults);
  registerCleanupFunction(() => {
    gAreas.get("nav-bar").set("defaultPlacements", backupDefaults);
    CustomizableUI.reset();
  });
  navbarDefaults.splice(
    navbarDefaults.indexOf("stop-reload-button") + 1,
    0,
    "sidebar-button"
  );
});

add_task(async function test_toolbar_sidebar_button() {
  ok(
    !document.getElementById("sidebar-button"),
    "Sidebar button is not showing in the toolbar initially."
  );
  let gFuturePlacements =
    CustomizableUI.getTestOnlyInternalProp("gFuturePlacements");
  is(
    gFuturePlacements.size,
    0,
    "All future placements should be dealt with by now."
  );
  CustomizableUIInternal._updateForNewVersion();
  is(
    gFuturePlacements.size,
    1,
    "Sidebar button should be included in gFuturePlacements"
  );
  ok(
    gFuturePlacements.get("nav-bar").has("sidebar-button"),
    "sidebar-button is added to the nav bar"
  );

  // Then place the item so it updates the saved state. This would normally happen
  // when we call `registerArea` at startup:
  CustomizableUIInternal._placeNewDefaultWidgetsInArea("nav-bar");

  // Check that we updated `gSavedState`'s placements.
  let navbarSavedPlacements =
    CustomizableUI.getTestOnlyInternalProp("gSavedState").placements["nav-bar"];
  Assert.ok(
    navbarSavedPlacements.includes("sidebar-button"),
    `${navbarSavedPlacements.join(", ")} should include sidebar-button`
  );
  // At startup we'd then transfer this into `gPlacements` from `restoreStateForArea`
  CustomizableUI.getTestOnlyInternalProp("gPlacements").set(
    "nav-bar",
    navbarSavedPlacements
  );
  // Check it's now actually present if we open a new window.
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document: newDoc } = win;
  ok(
    newDoc.getElementById("sidebar-button"),
    "Should have button in new window"
  );
  await BrowserTestUtils.closeWindow(win);
});
