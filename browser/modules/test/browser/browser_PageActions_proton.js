/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests PageActions.jsm with regard to Proton.

"use strict";

const PROTON_PREF = "browser.proton.urlbar.enabled";
const PERSISTED_ACTIONS_PREF = "browser.pageActions.persistedActions";
const TEST_ACTION_ID = "browser_PageActions_proton";

// Set of action IDs to ignore during this test.
//
// Pocket and Screenshots are problematic for two reasons: (1) They're going
// away in Proton, (2) they're built-in actions but they add themselves
// dynamically outside of PageActions, which is a problem for this test since it
// simulates app restart by removing all actions and adding them back.  So just
// don't make any assertions about either of them.
//
// addSearchEngine gets in the way of the test when it assumes for simplicity
// that all actions are pinned when Proton is enabled, but addSearchEngine still
// disables itself when the current page doesn't offer a search engine.  Bug
// 1697191 removes addSearchEngine and most of the other built-in actions, but
// until it lands, just ignore addSearchEngine.
const IGNORED_ACTION_IDS = new Set([
  "addSearchEngine",
  "pocket",
  "screenshots_mozilla_org",
]);
const EXTRA_IGNORED_ELEMENT_IDS = ["pocket-button"];

// This test opens and closes lots of windows and can often cause TV tests to
// time out for no good reason.  Request a ridiculously long timeout.
requestLongerTimeout(10);

add_task(async function init() {
  await disableNonReleaseActions();

  // This test toggles Proton and simulates restart a bunch of times.  We need
  // to leave PageActions in its initial state with its initial actions when the
  // test is done.
  let initialActions = PageActions.actions;
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(PERSISTED_ACTIONS_PREF);
    PageActions.init(false);
    for (let action of initialActions) {
      if (!PageActions.actionForID(action.id)) {
        PageActions.addAction(action);
      }
    }
  });
});

// New profile without Proton -> enable Proton -> disable Proton
add_task(async function newProfile_noProton_proton_noProton() {
  // Simulate restart with a new profile and Proton disabled.
  await SpecialPowers.pushPrefEnv({
    set: [[PROTON_PREF, false]],
  });
  simulateRestart({ newProfile: true });
  Assert.deepEqual(
    ids(PageActions._persistedActions.ids),
    ids(PageActions.actions),
    "Initial PageActions._persistedActions.ids"
  );
  Assert.deepEqual(
    ids(PageActions._persistedActions.idsInUrlbar, false),
    ids([PageActions.ACTION_ID_BOOKMARK], false),
    "Inital PageActions._persistedActions.idsInUrlbar"
  );
  Assert.ok(
    !PageActions._persistedActions.idsInUrlbarPreProton,
    "Initial PageActions._persistedActions.idsInUrlbarPreProton"
  );

  // Get the built-in copyURL action and pin it.
  let copyURLAction = PageActions.actionForID("copyURL");
  Assert.ok(copyURLAction, "copyURL action exists when Proton is disabled");
  copyURLAction.pinnedToUrlbar = true;

  // Add a pinned non-built-in action.
  let testAction = new PageActions.Action({
    id: TEST_ACTION_ID,
    pinnedToUrlbar: true,
  });
  PageActions.addAction(testAction);

  // Restart again, nothing should change.
  simulateRestart({ restoringActions: [testAction] });
  let idsInUrlbar = [
    copyURLAction.id,
    testAction.id,
    PageActions.ACTION_ID_BOOKMARK,
  ];
  await assertProtonNotApplied({ idsInUrlbar });

  // Enable Proton.  Nothing should change before restart.
  await SpecialPowers.pushPrefEnv({
    set: [[PROTON_PREF, true]],
  });
  await assertProtonNotApplied({ idsInUrlbar });

  // Restart.  idsInUrlbarPreProton should be the old idsInUrlbar.  copyURL will
  // remain in the new idsInUrlbar until the next restart since it wasn't
  // missing at the time of the previous restart and therefore wasn't purged,
  // even though it doesn't exist anymore.
  simulateRestart({ restoringActions: [testAction] });
  await assertProtonApplied({
    idsInUrlbarPreProton: idsInUrlbar,
    idsInUrlbarExtra: [copyURLAction.id],
  });

  // Restart again, nothing should change except copyURL is purged from
  // idsInUrlbar.
  simulateRestart({ restoringActions: [testAction] });
  await assertProtonApplied({ idsInUrlbarPreProton: idsInUrlbar });

  // Re-disable Proton.  Nothing should change before restart.
  await SpecialPowers.popPrefEnv();
  await assertProtonApplied({ idsInUrlbarPreProton: idsInUrlbar });

  // Restart.  The original idsInUrlbar should be restored, including copURL.
  simulateRestart({ restoringActions: [testAction] });
  await assertProtonNotApplied({ idsInUrlbar });

  testAction.remove();
  await SpecialPowers.popPrefEnv();
});

// New profile with Proton -> disable Proton -> re-enable Proton
add_task(async function newProfile_proton_noProton_proton() {
  // Simulate restart with a new profile and Proton enabled.
  await SpecialPowers.pushPrefEnv({
    set: [[PROTON_PREF, true]],
  });
  simulateRestart({ newProfile: true });
  Assert.deepEqual(
    ids(PageActions._persistedActions.ids),
    ids(PageActions.actions),
    "Initial PageActions._persistedActions.ids"
  );
  Assert.deepEqual(
    ids(PageActions._persistedActions.idsInUrlbar),
    ids(PageActions.actions.filter(a => !a.__isSeparator)),
    "Inital PageActions._persistedActions.idsInUrlbar"
  );
  Assert.equal(
    PageActions._persistedActions.idsInUrlbar[
      PageActions._persistedActions.idsInUrlbar.length - 1
    ],
    PageActions.ACTION_ID_BOOKMARK,
    "Inital PageActions._persistedActions.idsInUrlbar has bookmark action last"
  );
  Assert.deepEqual(
    PageActions._persistedActions.idsInUrlbarPreProton,
    [],
    "Initial PageActions._persistedActions.idsInUrlbarPreProton"
  );

  // Add a pinned non-built-in action.
  let testAction = new PageActions.Action({
    id: TEST_ACTION_ID,
    pinnedToUrlbar: true,
  });
  PageActions.addAction(testAction);

  // Restart again, nothing should change.
  simulateRestart({ restoringActions: [testAction] });
  await assertProtonApplied({ idsInUrlbarPreProton: [] });

  // Disable Proton.  Nothing should change before restart.
  await SpecialPowers.pushPrefEnv({
    set: [[PROTON_PREF, false]],
  });
  await assertProtonApplied({ idsInUrlbarPreProton: [] });

  // Restart.  The built-in actions that are pinned by default should be pinned.
  // The test action will not be pinned because idsInUrlbarPreProton is empty.
  simulateRestart({ restoringActions: [testAction] });
  let idsInUrlbar = [PageActions.ACTION_ID_BOOKMARK];
  await assertProtonNotApplied({ idsInUrlbar });

  testAction.pinnedToUrlbar = true;

  // Restart again.  The test action should be pinned.
  simulateRestart({ restoringActions: [testAction] });
  idsInUrlbar.splice(idsInUrlbar.length - 1, 0, testAction.id);
  await assertProtonNotApplied({ idsInUrlbar });

  // Re-enable Proton.  Nothing should change before restart.
  await SpecialPowers.popPrefEnv();
  await assertProtonNotApplied({ idsInUrlbar });

  // Restart.
  simulateRestart({ restoringActions: [testAction] });
  await assertProtonApplied({ idsInUrlbarPreProton: idsInUrlbar });

  testAction.remove();
  await SpecialPowers.popPrefEnv();
});

/**
 * Simulates an app restart by purging unregistered actions, storing persisted
 * actions, removing all actions, and then re-initializing PageActions.
 *
 * @param {boolean} newProfile
 *   Pass true to simulate a new profile as far as PageActions is concerned by
 *   clearing PERSISTED_ACTIONS_PREF before calling init().  Pass false to
 *   simulate a usual restart.
 * @param {array} restoringActions
 *   Any actions that should be added back after the simulated restart.
 *   PageActions automatically adds built-in actions on init, but any other
 *   actions must be added back manually.
 */
function simulateRestart({ newProfile = false, restoringActions = [] } = {}) {
  info("Simulating restart...");

  // shutdown phase
  // _purgeUnregisteredPersistedActions also stores the persisted actions.
  PageActions._purgeUnregisteredPersistedActions();
  for (let action of PageActions.actions) {
    action.remove();
  }

  // startup phase
  if (newProfile) {
    Services.prefs.clearUserPref(PERSISTED_ACTIONS_PREF);
  }
  PageActions.init(false);
  for (let action of restoringActions) {
    if (!PageActions.actionForID(action.id)) {
      PageActions.addAction(action);
    }
  }

  info(
    "Simulating restart done, PageActions._persistedActions=" +
      JSON.stringify(PageActions._persistedActions)
  );
}

/**
 * Asserts that PageActions recognizes Proton is enabled.  This isn't the same
 * as whether Proton is actually enabled because PageActions responds to changes
 * in the Proton pref only at the time it's initialized.
 *
 * @param {array} idsInUrlbarPreProton
 *   The expected value of PageActions._persistedActions.idsInUrlbarPreProton.
 * @param {array} idsInUrlbarExtra
 *   IDs that are expected to be in idsInUrlbar even though their actions are
 *   not registered.
 */
async function assertProtonApplied({
  idsInUrlbarPreProton,
  idsInUrlbarExtra = [],
}) {
  info("Asserting Proton is applied...");

  Assert.deepEqual(
    ids(PageActions._persistedActions.idsInUrlbar),
    ids(
      PageActions.actions.filter(a => !a.__isSeparator).concat(idsInUrlbarExtra)
    ),
    "idsInUrlbar when Proton applied"
  );

  if (!idsInUrlbarPreProton) {
    Assert.ok(
      !PageActions._persistedActions.idsInUrlbarPreProton,
      "idsInUrlbarPreProton does not exist when Proton applied"
    );
  } else {
    Assert.deepEqual(
      ids(PageActions._persistedActions.idsInUrlbarPreProton),
      ids(idsInUrlbarPreProton),
      "idsInUrlbarPreProton when Proton applied"
    );
  }

  assertCommonState({ idsInUrlbarExtra });
  await checkNewWindow({ idsInUrlbarExtra });
}

/**
 * Asserts that PageActions recognizes Proton is disabled.  This isn't the same
 * as whether Proton is actually disabled because PageActions responds to
 * changes in the Proton pref only at the time it's initialized.
 *
 * @param {array} idsInUrlbar
 *   The expected value of PageActions._persistedActions.idsInUrlbar.
 */
async function assertProtonNotApplied({ idsInUrlbar }) {
  info("Asserting Proton is not applied...");
  Assert.deepEqual(
    ids(PageActions._persistedActions.idsInUrlbar, false),
    ids(idsInUrlbar, false),
    "idsInUrlbar when Proton not applied"
  );
  Assert.ok(
    !PageActions._persistedActions.idsInUrlbarPreProton,
    "idsInUrlbarPreProton does not exist when Proton not applied"
  );
  assertCommonState();
  await checkNewWindow();
}

/**
 * Makes some assertions about PageActions state during this test that should be
 * the same regardless of whether Proton is enabled or disabled.
 *
 * @param {array} idsInUrlbarExtra
 *   IDs that are expected to be in idsInUrlbar even though their actions are
 *   not registered.
 */
function assertCommonState({ idsInUrlbarExtra = [] } = {}) {
  Assert.equal(
    PageActions._persistedActions.idsInUrlbar[
      PageActions._persistedActions.idsInUrlbar.length - 1
    ],
    PageActions.ACTION_ID_BOOKMARK,
    "Bookmark action is last in idsInUrlbar"
  );
  Assert.deepEqual(
    ids(PageActions.actionsInUrlbar(window), false),
    ids(
      PageActions._persistedActions.idsInUrlbar.filter(
        id => !idsInUrlbarExtra.includes(id)
      ),
      false
    ),
    "actionsInUrlbar matches idsInUrlbar"
  );
}

/**
 * Opens a new window and asserts that its state is correct for the current
 * PageActions and Proton states.
 *
 * @param {array} idsInUrlbarExtra
 *   IDs that are expected to be in idsInUrlbar even though their actions are
 *   not registered.
 */
async function checkNewWindow({ idsInUrlbarExtra = [] } = {}) {
  if (AppConstants.platform == "macosx" && AppConstants.DEBUG) {
    // Even with a bigger timeout than the one requested above, opening all
    // these windows still causes a timeout in Mac debug WebRender TV.  Just
    // skip this part of the test on Mac debug.
    info("Skipping checkNewWindow on Mac debug due to TV failures");
    return;
  }

  info("Checking a new window...");

  // Open a new window.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    url: "http://example.com/",
  });

  let ignoredElementIDs = Array.from(IGNORED_ACTION_IDS)
    .map(id => win.BrowserPageActions.urlbarButtonNodeIDForActionID(id))
    .concat(EXTRA_IGNORED_ELEMENT_IDS);

  // Collect its urlbar element IDs.
  let elementIDs = [];
  for (
    let element = win.BrowserPageActions.mainButtonNode.nextElementSibling;
    element;
    element = element.nextElementSibling
  ) {
    if (!ignoredElementIDs.includes(element.id)) {
      elementIDs.push(element.id);
    }
  }

  // Check that they match idsInUrlbar.
  Assert.deepEqual(
    elementIDs,
    ids(
      PageActions._persistedActions.idsInUrlbar.filter(
        id => !idsInUrlbarExtra.includes(id)
      ),
      false
    ).map(id => win.BrowserPageActions.urlbarButtonNodeIDForActionID(id)),
    "Actions in new window's urlbar"
  );

  await BrowserTestUtils.closeWindow(win);
}

/**
 * Convenience function that returns action IDs with IGNORED_ACTION_IDS removed
 * and optionally sorted.  See the IGNORED_ACTION_IDS comment for info on
 * ignored IDs.
 *
 * @param {array} idsOrActions
 *   Array of action IDs or actions.
 * @param {boolean} sort
 *   Pass true if the ordering of the IDs isn't important and you want to
 *   deepEqual() two arrays of IDs as if they were sets.  Pass false to preserve
 *   the current ordering.
 */
function ids(idsOrActions, sort = true) {
  let array = idsOrActions
    .map(obj => (typeof obj == "string" ? obj : obj.id))
    .filter(id => !IGNORED_ACTION_IDS.has(id));
  if (sort) {
    array = array.sort();
  }
  return array;
}
