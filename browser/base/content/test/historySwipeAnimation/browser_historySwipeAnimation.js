/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  BrowserOpenTab();
  let tab = gBrowser.selectedTab;
  registerCleanupFunction(function() { gBrowser.removeTab(tab); });

  ok(gHistorySwipeAnimation, "gHistorySwipeAnimation exists.");

  if (!gHistorySwipeAnimation._isSupported()) {
    is(gHistorySwipeAnimation.active, false, "History swipe animation is not " +
       "active when not supported by the platform.");
    finish();
    return;
  }

  gHistorySwipeAnimation.init();

  is(gHistorySwipeAnimation.active, true, "History swipe animation support " +
     "was successfully initialized when supported.");

  test0();

  function test0() {
    // Test uninit of gHistorySwipeAnimation.
    // This test MUST be the last one to execute.
    gHistorySwipeAnimation.uninit();
    is(gHistorySwipeAnimation.active, false, "History swipe animation support " +
       "was successfully uninitialized");
    finish();
  }
}
