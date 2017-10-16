/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test ensures that closed tabs are merged when restoring
 * a window state without overwriting tabs.
 */
add_task(async function() {
  const initialState = {
    windows: [{
      tabs: [
        { entries: [{ url: "about:blank", triggeringPrincipal_base64 }] }
      ],
      _closedTabs: [
        { state: { entries: [{ ID: 1000, url: "about:blank", triggeringPrincipal_base64 }]} },
        { state: { entries: [{ ID: 1001, url: "about:blank", triggeringPrincipal_base64 }]} }
      ]
    }]
  };

  const restoreState = {
    windows: [{
      tabs: [
        { entries: [{ url: "about:robots", triggeringPrincipal_base64 }] }
      ],
      _closedTabs: [
        { state: { entries: [{ ID: 1002, url: "about:robots", triggeringPrincipal_base64 }]} },
        { state: { entries: [{ ID: 1003, url: "about:robots", triggeringPrincipal_base64 }]} },
        { state: { entries: [{ ID: 1004, url: "about:robots", triggeringPrincipal_base64 }]} }
      ]
    }]
  };

  const maxTabsUndo = 4;
  gPrefService.setIntPref("browser.sessionstore.max_tabs_undo", maxTabsUndo);

  // Open a new window and restore it to an initial state.
  let win = await promiseNewWindowLoaded({private: false});
  SessionStore.setWindowState(win, JSON.stringify(initialState), true);
  is(SessionStore.getClosedTabCount(win), 2, "2 closed tabs after restoring initial state");

  // Restore the new state but do not overwrite existing tabs (this should
  // cause the closed tabs to be merged).
  SessionStore.setWindowState(win, JSON.stringify(restoreState), false);

  // Verify the windows closed tab data is correct.
  let iClosed = initialState.windows[0]._closedTabs;
  let rClosed = restoreState.windows[0]._closedTabs;
  let cData = JSON.parse(SessionStore.getClosedTabData(win));

  is(cData.length, Math.min(iClosed.length + rClosed.length, maxTabsUndo),
     "Number of closed tabs is correct");

  // When the closed tabs are merged the restored tabs are considered to be
  // closed more recently.
  for (let i = 0; i < cData.length; i++) {
    if (i < rClosed.length) {
      is(cData[i].state.entries[0].ID, rClosed[i].state.entries[0].ID,
         "Closed tab entry matches");
    } else {
      is(cData[i].state.entries[0].ID, iClosed[i - rClosed.length].state.entries[0].ID,
         "Closed tab entry matches");
    }
  }

  // Clean up.
  gPrefService.clearUserPref("browser.sessionstore.max_tabs_undo");
  await BrowserTestUtils.closeWindow(win);
});


