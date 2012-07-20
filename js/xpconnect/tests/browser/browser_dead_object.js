/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// For bug 773980, test that Components.utils.isDeadWrapper works as expected.
function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab("http://example.com");
  let tabBrowser = tab.linkedBrowser;

  tabBrowser.addEventListener("load", function loadListener(aEvent) {
    tabBrowser.removeEventListener("load", loadListener, true);

    let contentWindow = tab.linkedBrowser.contentWindow;
    gBrowser.removeTab(tab);

    SimpleTest.executeSoon(function() {
      ok(Components.utils.isDeadWrapper(contentWindow),
         'Window should be dead.');
      finish();
    });
  }, true);
}
