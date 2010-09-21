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

function browserWindowsCount() {
  let count = 0;
  let e = Services.wm.getEnumerator("navigator:browser");
  while (e.hasMoreElements()) {
    if (!e.getNext().closed)
      ++count;
  }
  return count;
}

function test() {
  /** Test for Bug 484108 **/
  is(browserWindowsCount(), 1, "Only one browser window should be open initially");

  let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
  waitForExplicitFinish();

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

  // builds an array of the indexs we expect to see in the order they get loaded
  function buildExpectedOrder(num, selected, shown) {
    // assume selected is 1-based index
    selected--;
    let expected = [selected];
    // fill left to selected if space
    for (let i = selected - (shown - expected.length); i >= 0 && i < selected; i++)
      expected.push(i);
    // fill from left to right until right length or end
    for (let i = selected + 1; expected.length < shown && i < num; i++)
      expected.push(i);
    // fill in the remaining
    for (let i = 0; i < num; i++) {
      if (expected.indexOf(i) == -1) {
        expected.push(i);
      }
    }
    return expected;
  }

  // the number of tests we're running
  let numTests = 6;
  let completedTests = 0;

  let tabMinWidth = parseInt(getComputedStyle(gBrowser.selectedTab, null).minWidth);

  function runTest(testNum, totalTabs, selectedTab, shownTabs, hiddenTabs, order) {
    let test = {
      state: buildTestState(totalTabs, selectedTab, hiddenTabs),
      numTabsToShow: shownTabs,
      expectedOrder: order,
      actualOrder: [],
      windowWidth: null,
      callback: null,
      window: null,

      handleSSTabRestoring: function (aEvent) {
        let tab = aEvent.originalTarget;
        let tabbrowser = this.window.gBrowser;
        let currentIndex = Array.indexOf(tabbrowser.tabs, tab);
        this.actualOrder.push(currentIndex);

        if (this.actualOrder.length < this.state.windows[0].tabs.length)
          return;

        // all of the tabs should be restoring or restored by now
        is(this.actualOrder.length, this.state.windows[0].tabs.length,
           "Test #" + testNum + ": Number of restored tabs is as expected");

        is(this.actualOrder.join(" "), this.expectedOrder.join(" "),
           "Test #" + testNum + ": 'visible' tabs restored first");

        // cleanup
        this.window.close();
        // if we're all done, explicitly finish
        if (++completedTests == numTests) {
          this.window.removeEventListener("load", this, false);
          this.window.removeEventListener("SSTabRestoring", this, false);
          is(browserWindowsCount(), 1, "Only one browser window should be open eventually");
          finish();
        }
      },

      handleLoad: function (aEvent) {
        let _this = this;
        executeSoon(function () {
          _this.window.resizeTo(_this.windowWidth, _this.window.outerHeight);
          ss.setWindowState(_this.window, JSON.stringify(_this.state), true);
        });
      },

      // Implement nsIDOMEventListener for handling various window and tab events
      handleEvent: function (aEvent) {
        switch (aEvent.type) {
          case "load":
            this.handleLoad(aEvent);
            break;
          case "SSTabRestoring":
            this.handleSSTabRestoring(aEvent);
            break;
        }
      },

      // setup and actually run the test
      run: function () {
        this.windowWidth = Math.floor((this.numTabsToShow - 0.5) * tabMinWidth);
        this.window = openDialog(location, "_blank", "chrome,all,dialog=no");
        this.window.addEventListener("SSTabRestoring", this, false);
        this.window.addEventListener("load", this, false);
      }
    };
    test.run();
  }

  // actually create & run the tests
  runTest(1, 13, 1,  6, [],         [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]);
  runTest(2, 13, 13, 6, [],         [12, 7, 8, 9, 10, 11, 0, 1, 2, 3, 4, 5, 6]);
  runTest(3, 13, 4,  6, [],         [3, 4, 5, 6, 7, 8, 0, 1, 2, 9, 10, 11, 12]);
  runTest(4, 13, 11, 6, [],         [10, 7, 8, 9, 11, 12, 0, 1, 2, 3, 4, 5, 6]);
  runTest(5, 13, 13, 6, [0, 4, 9],  [12, 6, 7, 8, 10, 11, 1, 2, 3, 5, 0, 4, 9]);
  runTest(6, 13, 4,  6, [1, 7, 12], [3, 4, 5, 6, 8, 9, 0, 2, 10, 11, 1, 7, 12]);

  // finish() is run by the last test to finish, so no cleanup down here
}
