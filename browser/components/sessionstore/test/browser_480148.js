/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Paul O’Shannessy <poshannessy@mozilla.com> (primary author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
