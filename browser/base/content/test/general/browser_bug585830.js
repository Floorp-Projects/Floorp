/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  let tab1 = gBrowser.selectedTab;
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank", {skipAnimation: true});
  BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = tab2;

  gBrowser.removeCurrentTab({animate: true});
  gBrowser.tabContainer.advanceSelectedTab(-1, true);
  is(gBrowser.selectedTab, tab1, "First tab should be selected");
  gBrowser.removeTab(tab2);

  // test for "null has no properties" fix. See Bug 585830 Comment 13
  gBrowser.removeCurrentTab({animate: true});
  try {
    gBrowser.tabContainer.advanceSelectedTab(-1, false);
  } catch (err) {
    ok(false, "Shouldn't throw");
  }

  gBrowser.removeTab(tab1);
}
