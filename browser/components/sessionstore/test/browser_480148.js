/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 484108 **/
  waitForExplicitFinish();
  requestLongerTimeout(3);

  // builds the tests state based on a few parameters
  function buildTestState(num, selected, hidden, pinned) {
    let state = { windows: [ { "tabs": [], "selected": selected + 1 } ] };
    while (num--) {
      state.windows[0].tabs.push({
        entries: [
          { url: "http://example.com/?t=" + state.windows[0].tabs.length }
        ]
      });
      let i = state.windows[0].tabs.length - 1;
      if (hidden.length > 0 && i == hidden[0]) {
        state.windows[0].tabs[i].hidden = true;
        hidden.splice(0, 1);
      }
      if (pinned.length > 0 && i == pinned[0]) {
        state.windows[0].tabs[i].pinned = true;
        pinned.splice(0, 1);
      }
    }
    return state;
  }

  let tests = [
    { testNum: 1,
      totalTabs: 13,
      selectedTab: 0,
      shownTabs: 6,
      hiddenTabs: [],
      pinnedTabs: [],
      order: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
    },
    { testNum: 2,
      totalTabs: 13,
      selectedTab: 12,
      shownTabs: 6,
      hiddenTabs: [],
      pinnedTabs: [],
      order: [12, 7, 8, 9, 10, 11, 0, 1, 2, 3, 4, 5, 6]
    },
    { testNum: 3,
      totalTabs: 13,
      selectedTab: 3,
      shownTabs: 6,
      hiddenTabs: [],
      pinnedTabs: [],
      order: [3, 4, 5, 6, 7, 8, 0, 1, 2, 9, 10, 11, 12]
    },
    { testNum: 4,
      totalTabs: 13,
      selectedTab: 10,
      shownTabs: 6,
      hiddenTabs: [],
      pinnedTabs: [],
      order: [10, 7, 8, 9, 11, 12, 0, 1, 2, 3, 4, 5, 6]
    },
    { testNum: 5,
      totalTabs: 13,
      selectedTab: 12,
      shownTabs: 6,
      hiddenTabs: [0, 4, 9],
      pinnedTabs: [],
      order: [12, 6, 7, 8, 10, 11, 1, 2, 3, 5, 0, 4, 9]
    },
    { testNum: 6,
      totalTabs: 13,
      selectedTab: 3,
      shownTabs: 6,
      hiddenTabs: [1, 7, 12],
      pinnedTabs: [],
      order: [3, 4, 5, 6, 8, 9, 0, 2, 10, 11, 1, 7, 12]
    },
    { testNum: 7,
      totalTabs: 13,
      selectedTab: 3,
      shownTabs: 6,
      hiddenTabs: [0, 1, 2],
      pinnedTabs: [],
      order: [3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0, 1, 2]
    },
    { testNum: 8,
      totalTabs: 13,
      selectedTab: 0,
      shownTabs: 6,
      hiddenTabs: [],
      pinnedTabs: [0],
      order: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
    },
    { testNum: 9,
      totalTabs: 13,
      selectedTab: 1,
      shownTabs: 6,
      hiddenTabs: [],
      pinnedTabs: [0],
      order: [1, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
    },
    { testNum: 10,
      totalTabs: 13,
      selectedTab: 3,
      shownTabs: 6,
      hiddenTabs: [2],
      pinnedTabs: [0,1],
      order: [3, 0, 1, 4, 5, 6, 7, 8, 9, 10, 11, 12, 2]
    },
    { testNum: 11,
      totalTabs: 13,
      selectedTab: 12,
      shownTabs: 6,
      hiddenTabs: [],
      pinnedTabs: [0,1,2],
      order: [12, 0, 1, 2, 7, 8, 9, 10, 11, 3, 4, 5, 6]
    },
    { testNum: 12,
      totalTabs: 13,
      selectedTab: 6,
      shownTabs: 6,
      hiddenTabs: [3,4,5],
      pinnedTabs: [0,1,2],
      order: [6, 0, 1, 2, 7, 8, 9, 10, 11, 12, 3, 4, 5]
    },
    { testNum: 13,
      totalTabs: 13,
      selectedTab: 1,
      shownTabs: 6,
      hiddenTabs: [3,4,5],
      pinnedTabs: [0,1,2],
      order: [1, 0, 2, 6, 7, 8, 9, 10, 11, 12, 3, 4, 5]
    },
    { testNum: 14,
      totalTabs: 13,
      selectedTab: 2,
      shownTabs: 6,
      hiddenTabs: [],
      pinnedTabs: [0,1,2],
      order: [2, 0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
    },
    { testNum: 15,
      totalTabs: 13,
      selectedTab: 3,
      shownTabs: 6,
      hiddenTabs: [1,4],
      pinnedTabs: [0,1,2],
      order: [3, 0, 1, 2, 5, 6, 7, 8, 9, 10, 11, 12, 4]
    }
  ];

  let tabMinWidth =
    parseInt(getComputedStyle(gBrowser.selectedTab, null).minWidth);
  let testIndex = 0;

  function runNextTest() {
    if (tests.length == 0) {
      finish();
      return;
    }

    info ("Starting test " + (++testIndex));
    let test = tests.shift();
    let state = buildTestState(test.totalTabs, test.selectedTab,
                               test.hiddenTabs, test.pinnedTabs);
    let tabbarWidth = Math.floor((test.shownTabs - 0.5) * tabMinWidth);
    let win = openDialog(location, "_blank", "chrome,all,dialog=no");
    let tabsRestored = [];

    win.addEventListener("SSTabRestoring", function onSSTabRestoring(aEvent) {
      let tab = aEvent.originalTarget;
      let tabLink = tab.linkedBrowser.currentURI.spec;
      let tabIndex =
        tabLink.substring(tabLink.indexOf("?t=") + 3, tabLink.length);

      // we need to compare with the tab's restoring index, no with the
      // position index, since the pinned tabs change the positions in the
      // tabbar. 
      tabsRestored.push(tabIndex);

      if (tabsRestored.length < state.windows[0].tabs.length)
        return;

      // all of the tabs should be restoring or restored by now
      is(tabsRestored.length, state.windows[0].tabs.length,
         "Test #" + testIndex + ": Number of restored tabs is as expected");

      is(tabsRestored.join(" "), test.order.join(" "),
         "Test #" + testIndex + ": 'visible' tabs restored first");

      // cleanup
      win.removeEventListener("SSTabRestoring", onSSTabRestoring, false);
      win.close();
      executeSoon(runNextTest);
    }, false);

    whenWindowLoaded(win, function(aEvent) {
      let extent =
        win.outerWidth - win.gBrowser.tabContainer.mTabstrip.scrollClientSize;
      let windowWidth = tabbarWidth + extent;
      win.resizeTo(windowWidth, win.outerHeight);
      ss.setWindowState(win, JSON.stringify(state), true);
    });
  };

  runNextTest();
}
