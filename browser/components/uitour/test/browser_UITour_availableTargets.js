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
    "devtools",
    "help",
    ...(CustomizableUI.protonToolbarEnabled ? [] : ["home"]),
    "library",
    "logins",
    "pageAction-bookmark",
    ...(gProton
      ? []
      : [
          "pageAction-copyURL",
          "pageAction-emailLink",
          "pageAction-sendToDevice",
        ]),
    ...(hasPocket ? ["pocket"] : []),
    "privateWindow",
    ...(hasQuit ? ["quit"] : []),
    "readerMode-urlBar",
    "urlbar",
  ];
}

add_task(setup_UITourTest);

add_UITour_task(async function test_availableTargets() {
  let data = await getConfigurationPromise("availableTargets");
  let expecteds = getExpectedTargets();
  ok_targets(data, expecteds);
  ok(UITour.availableTargetsCache.has(window), "Targets should now be cached");
});

add_UITour_task(async function test_availableTargets_changeWidgets() {
  CustomizableUI.addWidgetToArea(
    "bookmarks-menu-button",
    CustomizableUI.AREA_NAVBAR,
    0
  );
  ok(
    !UITour.availableTargetsCache.has(window),
    "Targets should be evicted from cache after widget change"
  );
  let data = await getConfigurationPromise("availableTargets");
  let expecteds = getExpectedTargets();
  expecteds = ["bookmarks", ...expecteds];
  ok_targets(data, expecteds);

  ok(
    UITour.availableTargetsCache.has(window),
    "Targets should now be cached again"
  );
  CustomizableUI.reset();
  ok(
    !UITour.availableTargetsCache.has(window),
    "Targets should not be cached after reset"
  );
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

add_UITour_task(
  async function test_availableTargets_removeUrlbarPageActionsAll() {
    if (gProton) {
      ok(
        true,
        "In proton actions cannot be removed from or added to the URL bar."
      );
      return;
    }
    pageActionsHelper.setActionsUrlbarState(false);
    UITour.clearAvailableTargetsCache();
    let data = await getConfigurationPromise("availableTargets");
    let expecteds = getExpectedTargets();
    ok_targets(data, expecteds);
    let expectedActions = [
      ["pageAction-bookmark", "pageAction-panel-bookmark"],
      ["pageAction-copyURL", "pageAction-panel-copyURL"],
      ["pageAction-emailLink", "pageAction-panel-emailLink"],
      ["pageAction-sendToDevice", "pageAction-panel-sendToDevice"],
    ];
    for (let [targetName, expectedNodeId] of expectedActions) {
      await assertTargetNode(targetName, expectedNodeId);
    }
    pageActionsHelper.restoreActionsUrlbarState();
  }
);

add_UITour_task(async function test_availableTargets_addUrlbarPageActionsAll() {
  if (gProton) {
    ok(
      true,
      "In proton actions cannot be removed from or added to the URL bar."
    );
    return;
  }
  pageActionsHelper.setActionsUrlbarState(true);
  UITour.clearAvailableTargetsCache();
  let data = await getConfigurationPromise("availableTargets");
  let expecteds = getExpectedTargets();
  ok_targets(data, expecteds);
  let expectedActions = [
    ["pageAction-bookmark", "star-button-box"],
    ["pageAction-copyURL", "pageAction-urlbar-copyURL"],
    ["pageAction-emailLink", "pageAction-urlbar-emailLink"],
    ["pageAction-sendToDevice", "pageAction-urlbar-sendToDevice"],
  ];
  for (let [targetName, expectedNodeId] of expectedActions) {
    await assertTargetNode(targetName, expectedNodeId);
  }
  pageActionsHelper.restoreActionsUrlbarState();
});

function ok_targets(actualData, expectedTargets) {
  // Depending on how soon after page load this is called, the selected tab icon
  // may or may not be showing the loading throbber.  We can't be sure whether
  // it appears in the list of targets, so remove it.
  let index = actualData.targets.indexOf("selectedTabIcon");
  if (index != -1) {
    actualData.targets.splice(index, 1);
  }

  ok(Array.isArray(actualData.targets), "data.targets should be an array");
  actualData.targets.sort();
  expectedTargets.sort();
  Assert.deepEqual(
    actualData.targets,
    expectedTargets,
    "Targets should be as expected"
  );
  if (actualData.targets.toString() != expectedTargets.toString()) {
    for (let actualItem of actualData.targets) {
      if (!expectedTargets.includes(actualItem)) {
        ok(false, `${actualItem} was an unexpected target.`);
      }
    }
    for (let expectedItem of expectedTargets) {
      if (!actualData.targets.includes(expectedItem)) {
        ok(false, `${expectedItem} should have been a target.`);
      }
    }
  }
}

async function assertTargetNode(targetName, expectedNodeId) {
  let target = await UITour.getTarget(window, targetName);
  is(target.node.id, expectedNodeId, "UITour should get the right target node");
}

var pageActionsHelper = {
  setActionsUrlbarState(inUrlbar) {
    this._originalStates = [];
    PageActions._actionsByID.forEach(action => {
      this._originalStates.push([action, action.pinnedToUrlbar]);
      action.pinnedToUrlbar = inUrlbar;
    });
  },

  restoreActionsUrlbarState() {
    if (!this._originalStates) {
      return;
    }
    for (let [action, originalState] of this._originalStates) {
      action.pinnedToUrlbar = originalState;
    }
    this._originalStates = null;
  },
};
