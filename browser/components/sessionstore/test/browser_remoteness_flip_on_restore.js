"use strict";

/**
 * This set of tests checks that the remoteness is properly
 * set for each browser in a window when that window has
 * session state loaded into it.
 */

/**
 * Takes a SessionStore window state object for a single
 * window, sets the selected tab for it, and then returns
 * the object to be passed to SessionStore.setWindowState.
 *
 * @param state (object)
 *        The state to prepare to be sent to a window. This is
 *        state should just be for a single window.
 * @param selected (int)
 *        The 1-based index of the selected tab. Note that
 *        If this is 0, then the selected tab will not change
 *        from what's already selected in the window that we're
 *        sending state to.
 * @returns (object)
 *        The JSON encoded string to call
 *        SessionStore.setWindowState with.
 */
function prepareState(state, selected) {
  // We'll create a copy so that we don't accidentally
  // modify the caller's selected property.
  let copy = {};
  Object.assign(copy, state);
  copy.selected = selected;

  return {
    windows: [ copy ],
  };
}

const SIMPLE_STATE = {
  tabs: [
    { entries: [{ url: "http://example.com/", title: "title" }] },
    { entries: [{ url: "http://example.com/", title: "title" }] },
    { entries: [{ url: "http://example.com/", title: "title" }] },
  ],
  title: "",
  _closedTabs: [],
};

const PINNED_STATE = {
  tabs: [
    { entries: [{ url: "http://example.com/", title: "title" }], pinned: true },
    { entries: [{ url: "http://example.com/", title: "title" }], pinned: true },
    { entries: [{ url: "http://example.com/", title: "title" }] },
  ],
  title: "",
  _closedTabs: [],
};

/**
 * This is where most of the action is happening. This function takes
 * an Array of "test scenario" Objects and runs them. For each scenario, a
 * window is opened, put into some state, and then a new state is
 * loaded into that window. We then check to make sure that the
 * right things have happened in that window wrt remoteness flips.
 *
 * The schema for a testing scenario Object is as follows:
 *
 * initialRemoteness:
 *   an Array that represents the starting window. Each bool
 *   in the Array represents the window tabs in order. A "true"
 *   indicates that that tab should be remote. "false" if the tab
 *   should be non-remote.
 *
 * initialSelectedTab:
 *   The 1-based index of the tab that we want to select for the
 *   restored window. This is 1-based to avoid confusion with the
 *   selectedTab property described down below, though you probably
 *   want to set this to be greater than 0, since the initial window
 *   needs to have a defined initial selected tab. Because of this,
 *   the test will throw if initialSelectedTab is 0.
 *
 * stateToRestore:
 *   A JS Object for the state to send down to the window.
 *
 * selectedTab:
 *   The 1-based index of the tab that we want to select for the
 *   restored window. Leave this at 0 if you don't want to change
 *   the selection from the initial window state.
 *
 * expectedFlips:
 *   an Array that represents the window that we end up with after
 *   restoring state. Each bool in the Array represents the window tabs,
 *   in order. A "true" indicates that the tab should have flipped
 *   its remoteness once. "false" indicates that the tab should never
 *   have flipped remoteness. Note that any tab that flips its remoteness
 *   more than once will cause a test failure.
 *
 * expectedRemoteness:
 *   an Array that represents the window that we end up with after
 *   restoring state. Each bool in the Array represents the window
 *   tabs in order. A "true" indicates that the tab be remote, and
 *   a "false" indicates that the tab should be "non-remote". We
 *   need this Array in order to test pinned tabs which will also
 *   be loaded by default, and therefore should end up remote.
 *
 */
function* runScenarios(scenarios) {
  for (let scenario of scenarios) {
    // Let's make sure our scenario is sane first.
    Assert.equal(scenario.expectedFlips.length,
                 scenario.expectedRemoteness.length,
                 "All expected flips and remoteness needs to be supplied");
    Assert.ok(scenario.initialSelectedTab > 0,
              "You must define an initially selected tab");

    // First, we need to create the initial conditions, so we
    // open a new window to put into our starting state...
    let win = yield BrowserTestUtils.openNewBrowserWindow();
    let tabbrowser = win.gBrowser;
    Assert.ok(tabbrowser.selectedBrowser.isRemoteBrowser,
              "The initial browser should be remote.");
    // Now put the window into the expected initial state.
    for (let i = 0; i < scenario.initialRemoteness.length; ++i) {
      let tab;
      if (i > 0) {
        // The window starts with one tab, so we need to create
        // any of the additional ones required by this test.
        info("Opening a new tab");
        tab = yield BrowserTestUtils.openNewForegroundTab(tabbrowser)
      } else {
        info("Using the selected tab");
        tab = tabbrowser.selectedTab;
      }
      let browser = tab.linkedBrowser;
      let remotenessState = scenario.initialRemoteness[i];
      tabbrowser.updateBrowserRemoteness(browser, remotenessState);
    }

    // And select the requested tab.
    let tabToSelect = tabbrowser.tabs[scenario.initialSelectedTab - 1];
    if (tabbrowser.selectedTab != tabToSelect) {
      yield BrowserTestUtils.switchTab(tabbrowser, tabToSelect);
    }

    // Hook up an event listener to make sure that the right
    // tabs flip remoteness, and only once.
    let flipListener = {
      seenBeforeTabs: new Set(),
      seenAfterTabs: new Set(),
      handleEvent(e) {
        let index = Array.from(tabbrowser.tabs).indexOf(e.target);
        switch (e.type) {
          case "BeforeTabRemotenessChange":
            info(`Saw tab at index ${index} before remoteness flip`);
            if (this.seenBeforeTabs.has(e.target)) {
              Assert.ok(false, "Saw tab before remoteness flip more than once");
            }
            this.seenBeforeTabs.add(e.target);
            break;
          case "TabRemotenessChange":
            info(`Saw tab at index ${index} after remoteness flip`);
            if (this.seenAfterTabs.has(e.target)) {
              Assert.ok(false, "Saw tab after remoteness flip more than once");
            }
            this.seenAfterTabs.add(e.target);
            break;
        }
      },
    };

    win.addEventListener("BeforeTabRemotenessChange", flipListener);
    win.addEventListener("TabRemotenessChange", flipListener);

    // Okay, time to test!
    let state = prepareState(scenario.stateToRestore,
                             scenario.selectedTab);

    SessionStore.setWindowState(win, state, true);

    win.removeEventListener("BeforeTabRemotenessChange", flipListener);
    win.removeEventListener("TabRemotenessChange", flipListener);

    // Because we know that scenario.expectedFlips and
    // scenario.expectedRemoteness have the same length, we
    // can check that we satisfied both with the same loop.
    for (let i = 0; i < scenario.expectedFlips.length; ++i) {
      let expectedToFlip = scenario.expectedFlips[i];
      let expectedRemoteness = scenario.expectedRemoteness[i];
      let tab = tabbrowser.tabs[i];
      if (expectedToFlip) {
        Assert.ok(flipListener.seenBeforeTabs.has(tab),
                  `We should have seen tab at index ${i} before remoteness flip`);
        Assert.ok(flipListener.seenAfterTabs.has(tab),
                  `We should have seen tab at index ${i} after remoteness flip`);
      } else {
        Assert.ok(!flipListener.seenBeforeTabs.has(tab),
                  `We should not have seen tab at index ${i} before remoteness flip`);
        Assert.ok(!flipListener.seenAfterTabs.has(tab),
                  `We should not have seen tab at index ${i} after remoteness flip`);
      }

      Assert.equal(tab.linkedBrowser.isRemoteBrowser, expectedRemoteness,
                   "Should have gotten the expected remoteness " +
                   `for the tab at index ${i}`);
    }

    yield BrowserTestUtils.closeWindow(win);
  }
}

/**
 * Tests that if we restore state to browser windows with
 * a variety of initial remoteness states, that we only flip
 * the remoteness on the necessary tabs. For this particular
 * set of tests, we assume that tabs are restoring on demand.
 */
add_task(function*() {
  // This test opens and closes windows, which might bog down
  // a debug build long enough to time out the test, so we
  // extend the tolerance on timeouts.
  requestLongerTimeout(5);

  yield SpecialPowers.pushPrefEnv({
    "set": [["browser.sessionstore.restore_on_demand", true]],
  });

  const TEST_SCENARIOS = [
    // Only one tab in the new window, and it's remote. This
    // is the common case, since this is how restoration occurs
    // when the restored window is being opened.
    {
      initialRemoteness: [true],
      initialSelectedTab: 1,
      stateToRestore: SIMPLE_STATE,
      selectedTab: 3,
      // The initial tab is remote and should go into
      // the background state. The second and third tabs
      // are new and should be initialized non-remote.
      expectedFlips: [true, false, true],
      // Only the selected tab should be remote.
      expectedRemoteness: [false, false, true],
    },

    // A single remote tab, and this is the one that's going
    // to be selected once state is restored.
    {
      initialRemoteness: [true],
      initialSelectedTab: 1,
      stateToRestore: SIMPLE_STATE,
      selectedTab: 1,
      // The initial tab is remote and selected, so it should
      // not flip remoteness. The other two new tabs should
      // be non-remote by default.
      expectedFlips: [false, false, false],
      // Only the selected tab should be remote.
      expectedRemoteness: [true, false, false],
    },

    // A single remote tab which starts selected. We set the
    // selectedTab to 0 which is equivalent to "don't change
    // the tab selection in the window".
    {
      initialRemoteness: [true],
      initialSelectedTab: 1,
      stateToRestore: SIMPLE_STATE,
      selectedTab: 0,
      // The initial tab is remote and selected, so it should
      // not flip remoteness. The other two new tabs should
      // be non-remote by default.
      expectedFlips: [false, false, false],
      // Only the selected tab should be remote.
      expectedRemoteness: [true, false, false],
    },

    // An initially remote tab, but we're going to load
    // some pinned tabs now, and the pinned tabs should load
    // right away.
    {
      initialRemoteness: [true],
      initialSelectedTab: 1,
      stateToRestore: PINNED_STATE,
      selectedTab: 3,
      // The initial tab is pinned and will load right away,
      // so it should stay remote. The second tab is new
      // and pinned, so it should start remote and not flip.
      // The third tab is not pinned, but it is selected,
      // so it will start non-remote, and then flip remoteness.
      expectedFlips: [false, false, true],
      // Both pinned tabs and the selected tabs should all
      // end up being remote.
      expectedRemoteness: [true, true, true],
    },

    // A single non-remote tab.
    {
      initialRemoteness: [false],
      initialSelectedTab: 1,
      stateToRestore: SIMPLE_STATE,
      selectedTab: 2,
      // The initial tab is non-remote and should stay
      // that way. The second and third tabs are new and
      // should be initialized non-remote.
      expectedFlips: [false, true, false],
      // Only the selected tab should be remote.
      expectedRemoteness: [false, true, false],
    },

    // A mixture of remote and non-remote tabs.
    {
      initialRemoteness: [true, false, true],
      initialSelectedTab: 1,
      stateToRestore: SIMPLE_STATE,
      selectedTab: 3,
      // The initial tab is remote and should flip to non-remote
      // as it is put into the background. The second tab should
      // stay non-remote, and the third one should stay remote.
      expectedFlips: [true, false, false],
      // Only the selected tab should be remote.
      expectedRemoteness: [false, false, true],
    },

    // An initially non-remote tab, but we're going to load
    // some pinned tabs now, and the pinned tabs should load
    // right away.
    {
      initialRemoteness: [false],
      initialSelectedTab: 1,
      stateToRestore: PINNED_STATE,
      selectedTab: 3,
      // The initial tab is pinned and will load right away,
      // so it should flip remoteness. The second tab is new
      // and pinned, so it should start remote and not flip.
      // The third tab is not pinned, but it is selected,
      // so it will start non-remote, and then flip remoteness.
      expectedFlips: [true, false, true],
      // Both pinned tabs and the selected tabs should all
      // end up being remote.
      expectedRemoteness: [true, true, true],
    },
  ];

  yield* runScenarios(TEST_SCENARIOS);
});
