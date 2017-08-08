"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

var hasPocket = Services.prefs.getBoolPref("extensions.pocket.enabled");
var hasQuit = AppConstants.platform != "macosx";

requestLongerTimeout(2);

function getExpectedTargets() {
  return [
    "accountStatus",
    "addons",
    "appMenu",
    "backForward",
    "bookmarks",
    "bookmark-star-button",
    "customize",
    "devtools",
    "help",
    "home",
    "library",
    "pageActionButton",
    "pageAction-panel-bookmark",
    "pageAction-panel-copyURL",
    "pageAction-panel-emailLink",
    "pageAction-panel-sendToDevice",
      ...(hasPocket ? ["pocket"] : []),
    "privateWindow",
      ...(hasQuit ? ["quit"] : []),
    "readerMode-urlBar",
    "search",
    "searchIcon",
    "trackingProtection",
    "urlbar",
  ];
}

add_task(setup_UITourTest);

add_UITour_task(async function test_availableTargets() {
  let data = await getConfigurationPromise("availableTargets");
  let expecteds = getExpectedTargets();
  ok_targets(data, expecteds);

  ok(UITour.availableTargetsCache.has(window),
     "Targets should now be cached");
});

add_UITour_task(async function test_availableTargets_changeWidgets() {
  CustomizableUI.removeWidgetFromArea("bookmarks-menu-button");
  ok(!UITour.availableTargetsCache.has(window),
     "Targets should be evicted from cache after widget change");
  let data = await getConfigurationPromise("availableTargets");
  let expecteds = getExpectedTargets();
  expecteds = expecteds.filter(target => target != "bookmarks");
  ok_targets(data, expecteds);

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
  let expecteds = getExpectedTargets();
  // Default minus "search" and "searchIcon"
  expecteds = expecteds.filter(target => target != "search" && target != "searchIcon");
  ok_targets(data, expecteds);

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
