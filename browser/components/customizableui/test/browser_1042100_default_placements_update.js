/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function getSavedStatePlacements(area) {
  return CustomizableUI.getTestOnlyInternalProp("gSavedState").placements[area];
}

// NB: This uses some ugly hacks to get into the CUI module from elsewhere...
// don't try this at home, kids.
function test() {
  // Customize something to make sure stuff changed:
  CustomizableUI.addWidgetToArea(
    "save-page-button",
    CustomizableUI.AREA_NAVBAR
  );

  let oldState = CustomizableUI.getTestOnlyInternalProp("gSavedState");
  registerCleanupFunction(() =>
    CustomizableUI.setTestOnlyInternalProp("gSavedState", oldState)
  );

  let gFuturePlacements =
    CustomizableUI.getTestOnlyInternalProp("gFuturePlacements");
  is(
    gFuturePlacements.size,
    0,
    "All future placements should be dealt with by now."
  );

  let gPalette = CustomizableUI.getTestOnlyInternalProp("gPalette");
  let CustomizableUIInternal = CustomizableUI.getTestOnlyInternalProp(
    "CustomizableUIInternal"
  );
  CustomizableUIInternal._updateForNewVersion();
  is(gFuturePlacements.size, 0, "No change to future placements initially.");

  let currentVersion = CustomizableUI.getTestOnlyInternalProp("kVersion");

  // Add our widget to the defaults:
  let testWidgetNew = {
    id: "test-messing-with-default-placements-new",
    label: "Test messing with default placements - should be inserted",
    defaultArea: CustomizableUI.AREA_NAVBAR,
    introducedInVersion: currentVersion + 1,
  };

  let normalizedWidget = CustomizableUIInternal.normalizeWidget(
    testWidgetNew,
    CustomizableUI.SOURCE_BUILTIN
  );
  ok(normalizedWidget, "Widget should be normalizable");
  if (!normalizedWidget) {
    return;
  }
  gPalette.set(testWidgetNew.id, normalizedWidget);

  let testWidgetOld = {
    id: "test-messing-with-default-placements-old",
    label: "Test messing with default placements - should NOT be inserted",
    defaultArea: CustomizableUI.AREA_NAVBAR,
    introducedInVersion: currentVersion,
  };

  normalizedWidget = CustomizableUIInternal.normalizeWidget(
    testWidgetOld,
    CustomizableUI.SOURCE_BUILTIN
  );
  ok(normalizedWidget, "Widget should be normalizable");
  if (!normalizedWidget) {
    return;
  }
  gPalette.set(testWidgetOld.id, normalizedWidget);

  // Now increase the version in the module:
  CustomizableUI.setTestOnlyInternalProp(
    "kVersion",
    CustomizableUI.getTestOnlyInternalProp("kVersion") + 1
  );

  let hadSavedState = !!CustomizableUI.getTestOnlyInternalProp("gSavedState");
  if (!hadSavedState) {
    CustomizableUI.setTestOnlyInternalProp("gSavedState", {
      currentVersion: CustomizableUI.getTestOnlyInternalProp("kVersion") - 1,
    });
  }

  // Then call the re-init routine so we re-add the builtin widgets
  CustomizableUIInternal._updateForNewVersion();
  is(gFuturePlacements.size, 1, "Should have 1 more future placement");
  let haveNavbarPlacements = gFuturePlacements.has(CustomizableUI.AREA_NAVBAR);
  ok(haveNavbarPlacements, "Should have placements for nav-bar");
  if (haveNavbarPlacements) {
    let placements = [...gFuturePlacements.get(CustomizableUI.AREA_NAVBAR)];

    // Ignore widgets that are placed using the pref facility and not the
    // versioned facility.  They're independent of kVersion and the saved
    // state's current version, so they may be present in the placements.
    for (let i = 0; i < placements.length; ) {
      if (placements[i] == testWidgetNew.id) {
        i++;
        continue;
      }
      let pref = "browser.toolbarbuttons.introduced." + placements[i];
      let introduced = Services.prefs.getBoolPref(pref, false);
      if (!introduced) {
        i++;
        continue;
      }
      placements.splice(i, 1);
    }

    is(placements.length, 1, "Should have 1 newly placed widget in nav-bar");
    is(
      placements[0],
      testWidgetNew.id,
      "Should have our test widget to be placed in nav-bar"
    );
  }

  // Reset kVersion
  CustomizableUI.setTestOnlyInternalProp(
    "kVersion",
    CustomizableUI.getTestOnlyInternalProp("kVersion") - 1
  );

  // Now test that the builtin photon migrations work:

  CustomizableUI.setTestOnlyInternalProp("gSavedState", {
    currentVersion: 6,
    placements: {
      "nav-bar": ["urlbar-container", "bookmarks-menu-button"],
      "PanelUI-contents": ["panic-button", "edit-controls"],
    },
  });
  Services.prefs.setIntPref("browser.proton.toolbar.version", 0);
  CustomizableUIInternal._updateForNewVersion();
  CustomizableUIInternal._updateForNewProtonVersion();
  {
    let navbarPlacements = getSavedStatePlacements("nav-bar");
    let springs = navbarPlacements.filter(id => id.includes("spring"));
    is(springs.length, 2, "Should have 2 toolbarsprings in placements now");
    navbarPlacements = navbarPlacements.filter(id => !id.includes("spring"));
    Assert.deepEqual(
      navbarPlacements,
      [
        "back-button",
        "forward-button",
        "stop-reload-button",
        "urlbar-container",
        "downloads-button",
        "fxa-toolbar-menu-button",
        "reset-pbm-toolbar-button",
      ],
      "Nav-bar placements should be correct."
    );

    Assert.deepEqual(getSavedStatePlacements("widget-overflow-fixed-list"), [
      "panic-button",
    ]);
  }

  // Finally, test the downloads and fxa avatar button migrations work.
  let oldNavbarPlacements = [
    "urlbar-container",
    "customizableui-special-spring3",
    "search-container",
  ];
  CustomizableUI.setTestOnlyInternalProp("gSavedState", {
    currentVersion: 10,
    placements: {
      "nav-bar": Array.from(oldNavbarPlacements),
      "widget-overflow-fixed-list": ["downloads-button"],
    },
  });
  CustomizableUIInternal._updateForNewVersion();
  Assert.deepEqual(
    getSavedStatePlacements("nav-bar"),
    oldNavbarPlacements.concat([
      "downloads-button",
      "fxa-toolbar-menu-button",
      "reset-pbm-toolbar-button",
    ]),
    "Downloads button inserted in navbar"
  );
  Assert.deepEqual(
    getSavedStatePlacements("widget-overflow-fixed-list"),
    [],
    "Overflow panel is empty"
  );

  CustomizableUI.setTestOnlyInternalProp("gSavedState", {
    currentVersion: 10,
    placements: {
      "nav-bar": ["downloads-button"].concat(oldNavbarPlacements),
    },
  });
  CustomizableUIInternal._updateForNewVersion();
  Assert.deepEqual(
    getSavedStatePlacements("nav-bar"),
    oldNavbarPlacements.concat([
      "downloads-button",
      "fxa-toolbar-menu-button",
      "reset-pbm-toolbar-button",
    ]),
    "Downloads button reinserted in navbar"
  );

  oldNavbarPlacements = [
    "urlbar-container",
    "customizableui-special-spring3",
    "search-container",
    "other-widget",
  ];
  CustomizableUI.setTestOnlyInternalProp("gSavedState", {
    currentVersion: 10,
    placements: {
      "nav-bar": Array.from(oldNavbarPlacements),
    },
  });
  CustomizableUIInternal._updateForNewVersion();
  let expectedNavbarPlacements = [
    "urlbar-container",
    "customizableui-special-spring3",
    "search-container",
    "downloads-button",
    "other-widget",
    "fxa-toolbar-menu-button",
    "reset-pbm-toolbar-button",
  ];
  Assert.deepEqual(
    getSavedStatePlacements("nav-bar"),
    expectedNavbarPlacements,
    "Downloads button inserted in navbar before other widgets"
  );

  gFuturePlacements.delete(CustomizableUI.AREA_NAVBAR);
  gPalette.delete(testWidgetNew.id);
  gPalette.delete(testWidgetOld.id);
}
