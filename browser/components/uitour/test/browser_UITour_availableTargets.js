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
    "customize",
    "devtools",
    "help",
    "home",
    "library",
    "pageActionButton",
    "pageAction-bookmark",
    "pageAction-copyURL",
    "pageAction-emailLink",
    "pageAction-sendToDevice",
      ...(hasPocket ? ["pocket"] : []),
    "privateWindow",
      ...(hasQuit ? ["quit"] : []),
    "readerMode-urlBar",
    "screenshots",
    "search",
    "searchIcon",
    "trackingProtection",
    "urlbar",
  ];
}

add_task(setup_UITourTest);

add_UITour_task(async function test_availableTargets() {
  await ensureScreenshotsEnabled();
  let data = await getConfigurationPromise("availableTargets");
  let expecteds = getExpectedTargets();
  ok_targets(data, expecteds);
  ok(UITour.availableTargetsCache.has(window),
     "Targets should now be cached");
});

add_UITour_task(async function test_availableTargets_changeWidgets() {
  CustomizableUI.addWidgetToArea("bookmarks-menu-button", CustomizableUI.AREA_NAVBAR, 0);
  ok(!UITour.availableTargetsCache.has(window),
     "Targets should be evicted from cache after widget change");
  let data = await getConfigurationPromise("availableTargets");
  let expecteds = getExpectedTargets();
  expecteds = ["bookmarks", ...expecteds];
  ok_targets(data, expecteds);

  ok(UITour.availableTargetsCache.has(window),
     "Targets should now be cached again");
  CustomizableUI.reset();
  ok(!UITour.availableTargetsCache.has(window),
     "Targets should not be cached after reset");
});

add_UITour_task(async function test_availableTargets_search() {
  Services.prefs.setBoolPref("browser.search.widget.inNavBar", true);
  try {
    let data = await getConfigurationPromise("availableTargets");
    let expecteds = getExpectedTargets();
    expecteds = ["search", "searchIcon", ...expecteds];
    ok_targets(data, expecteds);
  } finally {
    Services.prefs.clearUserPref("browser.search.widget.inNavBar");
  }
});

add_UITour_task(async function test_availableTargets_removeUrlbarPageActionsAll() {
  pageActionsHelper.setActionsUrlbarState(false);
  UITour.clearAvailableTargetsCache();
  let data = await getConfigurationPromise("availableTargets");
  let expecteds = getExpectedTargets();
  ok_targets(data, expecteds);
  let expectedActions = [
    [ "pocket", "pageAction-panel-pocket" ],
    [ "screenshots", "pageAction-panel-screenshots" ],
    [ "pageAction-bookmark", "pageAction-panel-bookmark" ],
    [ "pageAction-copyURL", "pageAction-panel-copyURL" ],
    [ "pageAction-emailLink", "pageAction-panel-emailLink" ],
    [ "pageAction-sendToDevice", "pageAction-panel-sendToDevice" ],
  ];
  for (let [ targetName, expectedNodeId ] of expectedActions) {
    await assertTargetNode(targetName, expectedNodeId);
  }
  pageActionsHelper.restoreActionsUrlbarState();
});

add_UITour_task(async function test_availableTargets_addUrlbarPageActionsAll() {
  pageActionsHelper.setActionsUrlbarState(true);
  UITour.clearAvailableTargetsCache();
  let data = await getConfigurationPromise("availableTargets");
  let expecteds = getExpectedTargets();
  ok_targets(data, expecteds);
  let expectedActions = [
    [ "pocket", "pocket-button-box" ],
    [ "screenshots", "pageAction-urlbar-screenshots" ],
    [ "pageAction-bookmark", "star-button-box" ],
    [ "pageAction-copyURL", "pageAction-urlbar-copyURL" ],
    [ "pageAction-emailLink", "pageAction-urlbar-emailLink" ],
    [ "pageAction-sendToDevice", "pageAction-urlbar-sendToDevice" ],
  ];
  for (let [ targetName, expectedNodeId ] of expectedActions) {
    await assertTargetNode(targetName, expectedNodeId);
  }
  pageActionsHelper.restoreActionsUrlbarState();
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

async function assertTargetNode(targetName, expectedNodeId) {
  let target = await UITour.getTarget(window, targetName);
  is(target.node.id, expectedNodeId, "UITour should get the right target node");
}

var pageActionsHelper = {
  setActionsUrlbarState(inUrlbar) {
    this._originalStates = [];
    PageActions._actionsByID.forEach(action => {
      this._originalStates.push([ action, action.shownInUrlbar ]);
      action.shownInUrlbar = inUrlbar;
    });
  },

  restoreActionsUrlbarState() {
    if (!this._originalStates) {
      return;
    }
    for (let [ action, originalState] of this._originalStates) {
      action.shownInUrlbar = originalState;
    }
    this._originalStates = null;
  }
};

function ensureScreenshotsEnabled() {
  SpecialPowers.pushPrefEnv({ set: [
    [ "extensions.screenshots.system", false ],
    [ "extensions.screenshots.system-disabled", false ]
  ]});
  return BrowserTestUtils.waitForCondition(() => {
    return PageActions.actionForID("screenshots") &&
           !CustomizableUI.getWidget("screenshots_mozilla_org-browser-action");
  }, "Should enable Screenshots");
}
