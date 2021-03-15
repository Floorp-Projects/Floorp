/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "HomePage",
  "resource:///modules/HomePage.jsm"
);

const kPrefHomePage = "browser.startup.homepage";
const kPrefExtensionControlled =
  "browser.startup.homepage_override.extensionControlled";
const kPrefProtonToolbarEnabled = "browser.proton.enabled";
const kPrefHomeButtonRemoved = "browser.engagement.home-button.has-removed";
const kHomeButtonId = "home-button";
const kUrlbarWidgetId = "urlbar-container";

async function withTestSetup({ protonEnabled = true } = {}, testFn) {
  CustomizableUI.removeWidgetFromArea(kHomeButtonId);

  await SpecialPowers.pushPrefEnv({
    set: [
      [kPrefProtonToolbarEnabled, protonEnabled],
      [kPrefHomeButtonRemoved, false],
      [kPrefHomePage, "about:home"],
      [kPrefExtensionControlled, false],
    ],
  });

  HomePage._addCustomizableUiListener();

  try {
    await testFn();
  } finally {
    await SpecialPowers.popPrefEnv();
    await CustomizableUI.reset();
  }
}

function assertHomeButtonInArea(area) {
  let placement = CustomizableUI.getPlacementOfWidget(kHomeButtonId);
  is(placement.area, area, "home button in area");
}

function assertHomeButtonNotPlaced() {
  ok(
    !CustomizableUI.getPlacementOfWidget(kHomeButtonId),
    "home button not placed"
  );
}

function assertHasRemovedPref(val) {
  is(
    Services.prefs.getBoolPref(kPrefHomeButtonRemoved),
    val,
    "Expected removed pref value"
  );
}

async function runAddButtonTest(protonEnabled) {
  await withTestSetup({}, async () => {
    Services.prefs.setBoolPref(kPrefProtonToolbarEnabled, protonEnabled);

    // Setting the homepage once should add to the toolbar.
    assertHasRemovedPref(false);
    assertHomeButtonNotPlaced();

    await HomePage.set("https://example.com/");

    if (protonEnabled) {
      assertHomeButtonInArea("nav-bar");
    } else {
      assertHomeButtonNotPlaced();
    }
    assertHasRemovedPref(false);

    // After removing the home button, a new homepage shouldn't add it.
    CustomizableUI.removeWidgetFromArea(kHomeButtonId);
    assertHasRemovedPref(protonEnabled);

    await HomePage.set("https://mozilla.org/");
    assertHomeButtonNotPlaced();
  });
}

add_task(async function testAddHomeButtonOnSet() {
  await runAddButtonTest(true);
});

add_task(async function testHomeButtonOnSetProtonOff() {
  await runAddButtonTest(false);
});

add_task(async function testHomeButtonDoesNotMove() {
  await withTestSetup({}, async () => {
    // Setting the homepage should not move the home button.
    CustomizableUI.addWidgetToArea(kHomeButtonId, "TabsToolbar");
    assertHasRemovedPref(false);
    assertHomeButtonInArea("TabsToolbar");

    await HomePage.set("https://example.com/");

    assertHasRemovedPref(false);
    assertHomeButtonInArea("TabsToolbar");
  });
});

add_task(async function testHomeButtonNotAddedBlank() {
  await withTestSetup({}, async () => {
    assertHomeButtonNotPlaced();
    assertHasRemovedPref(false);

    await HomePage.set("about:blank");

    assertHasRemovedPref(false);
    assertHomeButtonNotPlaced();

    await HomePage.set("about:home");

    assertHasRemovedPref(false);
    assertHomeButtonNotPlaced();
  });
});

add_task(async function testHomeButtonNotAddedExtensionControlled() {
  await withTestSetup({}, async () => {
    assertHomeButtonNotPlaced();
    assertHasRemovedPref(false);
    Services.prefs.setBoolPref(kPrefExtensionControlled, true);

    await HomePage.set("https://search.example.com/?q=%s");

    assertHomeButtonNotPlaced();
  });
});

add_task(async function testHomeButtonPlacement() {
  await withTestSetup({}, async () => {
    assertHomeButtonNotPlaced();
    HomePage._maybeAddHomeButtonToToolbar("https://example.com");
    let homePlacement = CustomizableUI.getPlacementOfWidget(kHomeButtonId);
    is(homePlacement.area, "nav-bar", "Home button is in the nav-bar");
    is(homePlacement.position, 3, "Home button is after stop/refresh");

    let addressBarPlacement = CustomizableUI.getPlacementOfWidget(
      kUrlbarWidgetId
    );
    is(
      addressBarPlacement.position,
      5,
      "There's a space between home and urlbar"
    );
    CustomizableUI.removeWidgetFromArea(kHomeButtonId);
    Services.prefs.setBoolPref(kPrefHomeButtonRemoved, false);

    try {
      CustomizableUI.addWidgetToArea(kUrlbarWidgetId, "nav-bar", 1);
      HomePage._maybeAddHomeButtonToToolbar("https://example.com");
      homePlacement = CustomizableUI.getPlacementOfWidget(kHomeButtonId);
      is(homePlacement.area, "nav-bar", "Home button is in the nav-bar");
      is(homePlacement.position, 1, "Home button is right before the urlbar");
    } finally {
      CustomizableUI.addWidgetToArea(
        kUrlbarWidgetId,
        addressBarPlacement.area,
        addressBarPlacement.position
      );
    }
  });
});
