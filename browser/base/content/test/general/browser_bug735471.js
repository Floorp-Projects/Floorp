/*
 * This Source Code Form is subject to the terms of the Mozilla Public License,
 * v. 2.0. If a copy of the MPL was not distributed with this file, You can
 * obtain one at http://mozilla.org/MPL/2.0/.
 */


function test() {
  waitForExplicitFinish();
  // Open a new tab.
  whenNewTabLoaded(window, testPreferences);
}

function testPreferences() {
  whenTabLoaded(gBrowser.selectedTab, function () {
    is(content.location.href, "about:preferences", "Checking if the preferences tab was opened");

    gBrowser.removeCurrentTab();
    finish();
  });

  openPreferences();
}
