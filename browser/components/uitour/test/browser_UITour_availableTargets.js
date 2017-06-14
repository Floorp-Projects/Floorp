"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

var hasPocket = Services.prefs.getBoolPref("extensions.pocket.enabled");
var isPhoton = Services.prefs.getBoolPref("browser.photon.structure.enabled");
var hasQuit = !isPhoton ||
              AppConstants.platform != "macosx";
var hasLibrary = isPhoton || false;

requestLongerTimeout(2);
add_task(setup_UITourTest);

add_UITour_task(async function test_availableTargets() {
  let data = await getConfigurationPromise("availableTargets");
  ok_targets(data, [
    "accountStatus",
    "addons",
    "appMenu",
    "backForward",
    "bookmarks",
    "customize",
    "devtools",
    "help",
    "home",
      ...(hasLibrary ? ["library"] : []),
      ...(hasPocket ? ["pocket"] : []),
    "privateWindow",
      ...(hasQuit ? ["quit"] : []),
    "readerMode-urlBar",
    "search",
    "searchIcon",
    "trackingProtection",
    "urlbar",
  ]);

  ok(UITour.availableTargetsCache.has(window),
     "Targets should now be cached");
});

add_UITour_task(async function test_availableTargets_changeWidgets() {
  CustomizableUI.removeWidgetFromArea("bookmarks-menu-button");
  ok(!UITour.availableTargetsCache.has(window),
     "Targets should be evicted from cache after widget change");
  let data = await getConfigurationPromise("availableTargets");
  ok_targets(data, [
    "accountStatus",
    "addons",
    "appMenu",
    "backForward",
    "customize",
    "help",
    "devtools",
    "home",
      ...(hasLibrary ? ["library"] : []),
      ...(hasPocket ? ["pocket"] : []),
    "privateWindow",
      ...(hasQuit ? ["quit"] : []),
    "readerMode-urlBar",
    "search",
    "searchIcon",
    "trackingProtection",
    "urlbar",
  ]);

  ok(UITour.availableTargetsCache.has(window),
     "Targets should now be cached again");
  CustomizableUI.reset();
  ok(!UITour.availableTargetsCache.has(window),
     "Targets should not be cached after reset");
});

add_UITour_task(async function test_availableTargets_exceptionFromGetTarget() {
  // The query function for the "search" target will throw if it's not found.
  // Make sure the callback still fires with the other available targets.
  CustomizableUI.removeWidgetFromArea("search-container");
  let data = await getConfigurationPromise("availableTargets");
  // Default minus "search" and "searchIcon"
  ok_targets(data, [
    "accountStatus",
    "addons",
    "appMenu",
    "backForward",
    "bookmarks",
    "customize",
    "devtools",
    "help",
    "home",
      ...(hasLibrary ? ["library"] : []),
      ...(hasPocket ? ["pocket"] : []),
    "privateWindow",
      ...(hasQuit ? ["quit"] : []),
    "readerMode-urlBar",
    "trackingProtection",
    "urlbar",
  ]);

  CustomizableUI.reset();
});

function ok_targets(actualData, expectedTargets) {
  // Depending on how soon after page load this is called, the selected tab icon
  // may or may not be showing the loading throbber.  Check for its presence and
  // insert it into expectedTargets if it's visible.
  let selectedTabIcon =
    document.getAnonymousElementByAttribute(gBrowser.selectedTab,
                                            "anonid",
                                            "tab-icon-image");
  if (selectedTabIcon && UITour.isElementVisible(selectedTabIcon))
    expectedTargets.push("selectedTabIcon");

  ok(Array.isArray(actualData.targets), "data.targets should be an array");
  is(actualData.targets.sort().toString(), expectedTargets.sort().toString(),
     "Targets should be as expected");
}
