/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Only windows with open tabs are restorable. Windows where a lone tab is
// detached may have _closedTabs, but is left with just an empty tab.
const STATES = [{
    shouldBeAdded: true,
    windowState: {
      windows: [{
        tabs: [{ entries: [{ url: "http://example.com", title: "example.com" }] }],
        selected: 1,
        _closedTabs: []
      }]
    }
  }, {
    shouldBeAdded: false,
    windowState: {
      windows: [{
        tabs: [{ entries: [] }],
        _closedTabs: []
      }]
    }
  }, {
    shouldBeAdded: false,
    windowState: {
      windows: [{
        tabs: [{ entries: [] }],
        _closedTabs: [{ state: { entries: [{ url: "http://example.com", index: 1 }] } }]
      }]
    }
  }, {
    shouldBeAdded: false,
    windowState: {
      windows: [{
        tabs: [{ entries: [] }],
        _closedTabs: [],
        extData: { keyname: "pi != " + Math.random() }
      }]
    }
  }];

add_task(function* test_bug_490040() {
  for (let state of STATES) {
    // Ensure we can store the window if needed.
    let startingClosedWindowCount = ss.getClosedWindowCount();
    yield pushPrefs(["browser.sessionstore.max_windows_undo",
                     startingClosedWindowCount + 1]);

    let curClosedWindowCount = ss.getClosedWindowCount();
    let win = yield BrowserTestUtils.openNewBrowserWindow();

    ss.setWindowState(win, JSON.stringify(state.windowState), true);
    if (state.windowState.windows[0].tabs.length) {
      yield BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    }

    yield BrowserTestUtils.closeWindow(win);

    is(ss.getClosedWindowCount(),
       curClosedWindowCount + (state.shouldBeAdded ? 1 : 0),
       "That window should " + (state.shouldBeAdded ? "" : "not ") +
       "be restorable");
  }
});
