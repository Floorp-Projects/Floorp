/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 484108 **/
  waitForExplicitFinish();
  requestLongerTimeout(3);

  // builds the tests state based on a few parameters
  function buildTestState(num, selected, hidden) {
    let state = { windows: [ { "tabs": [], "selected": selected } ] };
    while (num--) {
      state.windows[0].tabs.push({entries: [{url: "http://example.com/"}]});
      let i = state.windows[0].tabs.length - 1;
      if (hidden.length > 0 && i == hidden[0]) {
        state.windows[0].tabs[i].hidden = true;
        hidden.splice(0, 1);
      }
    }
    return state;
  }

  let tests = [
    { testNum: 1,
      totalTabs: 13,
      selectedTab: 1,
      shownTabs: 6,
      hiddenTabs: [],
      order: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
    },
    { testNum: 2,
      totalTabs: 13,
      selectedTab: 13,
      shownTabs: 6,
      hiddenTabs: [],
      order: [12, 7, 8, 9, 10, 11, 0, 1, 2, 3, 4, 5, 6]
    },
    { testNum: 3,
      totalTabs: 13,
      selectedTab: 4,
      shownTabs: 6,
      hiddenTabs: [],
      order: [3, 4, 5, 6, 7, 8, 0, 1, 2, 9, 10, 11, 12]
    },
    { testNum: 4,
      totalTabs: 13,
      selectedTab: 11,
      shownTabs: 6,
      hiddenTabs: [],
      order: [10, 7, 8, 9, 11, 12, 0, 1, 2, 3, 4, 5, 6]
    },
    { testNum: 5,
      totalTabs: 13,
      selectedTab: 13,
      shownTabs: 6,
      hiddenTabs: [0, 4, 9],
      order: [12, 6, 7, 8, 10, 11, 1, 2, 3, 5, 0, 4, 9]
    },
    { testNum: 6,
      totalTabs: 13,
      selectedTab: 4,
      shownTabs: 6,
      hiddenTabs: [1, 7, 12],
      order: [3, 4, 5, 6, 8, 9, 0, 2, 10, 11, 1, 7, 12]
    },
    { testNum: 7,
      totalTabs: 13,
      selectedTab: 4,
      shownTabs: 6,
      hiddenTabs: [0, 1, 2],
      order: [3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0, 1, 2]
    }
  ];

  let tabMinWidth = parseInt(getComputedStyle(gBrowser.selectedTab, null).minWidth);
  let testIndex = 0;

  function runNextTest() {
    if (tests.length == 0) {
      finish();
      return;
    }

    info ("Starting test " + (++testIndex));
    let test = tests.shift();
    let state = buildTestState(test.totalTabs, test.selectedTab, test.hiddenTabs);
    let tabbarWidth = Math.floor((test.shownTabs - 0.5) * tabMinWidth);
    let win = openDialog(location, "_blank", "chrome,all,dialog=no");
    let actualOrder = [];

    win.addEventListener("SSTabRestoring", function onSSTabRestoring(aEvent) {
      let tab = aEvent.originalTarget;
      let currentIndex = Array.indexOf(win.gBrowser.tabs, tab);
      actualOrder.push(currentIndex);

      if (actualOrder.length < state.windows[0].tabs.length)
        return;

      // all of the tabs should be restoring or restored by now
      is(actualOrder.length, state.windows[0].tabs.length,
         "Test #" + testIndex + ": Number of restored tabs is as expected");

      is(actualOrder.join(" "), test.order.join(" "),
         "Test #" + testIndex + ": 'visible' tabs restored first");

      // Cleanup.
      win.removeEventListener("SSTabRestoring", onSSTabRestoring, false);
      win.close();
      executeSoon(runNextTest);
    }, false);

    win.addEventListener("load", function onLoad(aEvent) {
      win.removeEventListener("load", onLoad, false);
      executeSoon(function () {
        let extent = win.outerWidth - win.gBrowser.tabContainer.mTabstrip.scrollClientSize;
        let windowWidth = tabbarWidth + extent;
        win.resizeTo(windowWidth, win.outerHeight);
        ss.setWindowState(win, JSON.stringify(state), true);
      });
    }, false);
  };

  runNextTest();
}
