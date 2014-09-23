/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;

Components.utils.import("resource:///modules/UITour.jsm");

function test() {
  requestLongerTimeout(2);
  UITourTest();
}

let tests = [
  function test_availableTargets(done) {
    gContentAPI.getConfiguration("availableTargets", (data) => {
      ok_targets(data, [
        "accountStatus",
        "addons",
        "appMenu",
        "backForward",
        "bookmarks",
        "customize",
        "help",
        "home",
        "pinnedTab",
        "privateWindow",
        "quit",
        "search",
        "searchProvider",
        "urlbar",
      ]);
      ok(UITour.availableTargetsCache.has(window),
         "Targets should now be cached");
      done();
    });
  },

  function test_availableTargets_changeWidgets(done) {
    CustomizableUI.removeWidgetFromArea("bookmarks-menu-button");
    ok(!UITour.availableTargetsCache.has(window),
       "Targets should be evicted from cache after widget change");
    gContentAPI.getConfiguration("availableTargets", (data) => {
      ok_targets(data, [
        "accountStatus",
        "addons",
        "appMenu",
        "backForward",
        "customize",
        "help",
        "home",
        "pinnedTab",
        "privateWindow",
        "quit",
        "search",
        "searchProvider",
        "urlbar",
      ]);
      ok(UITour.availableTargetsCache.has(window),
         "Targets should now be cached again");
      CustomizableUI.reset();
      ok(!UITour.availableTargetsCache.has(window),
         "Targets should not be cached after reset");
      done();
    });
  },

  function test_availableTargets_exceptionFromGetTarget(done) {
    // The query function for the "search" target will throw if it's not found.
    // Make sure the callback still fires with the other available targets.
    CustomizableUI.removeWidgetFromArea("search-container");
    gContentAPI.getConfiguration("availableTargets", (data) => {
      // Default minus "search" and "searchProvider"
      ok_targets(data, [
        "accountStatus",
        "addons",
        "appMenu",
        "backForward",
        "bookmarks",
        "customize",
        "help",
        "home",
        "pinnedTab",
        "privateWindow",
        "quit",
        "urlbar",
      ]);
      CustomizableUI.reset();
      done();
    });
  },
];

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
