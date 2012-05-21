/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  // Create a new tab and load about:addons
  let blanktab = gBrowser.addTab();
  gBrowser.selectedTab = blanktab;
  BrowserOpenAddonsMgr();

  is(blanktab, gBrowser.selectedTab, "Current tab should be blank tab");
  // Verify that about:addons loads
  waitForExplicitFinish();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    let browser = blanktab.linkedBrowser;
    is(browser.currentURI.spec, "about:addons", "about:addons should load into blank tab.");
    gBrowser.removeTab(blanktab);
    finish();
  }, true);
}
