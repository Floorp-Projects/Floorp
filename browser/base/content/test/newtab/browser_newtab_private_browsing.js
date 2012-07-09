/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests ensure that all changes made to the new tab page in private
 * browsing mode are discarded after switching back to normal mode again.
 * The private browsing mode should start with the current grid shown in normal
 * mode.
 */
let pb = Cc["@mozilla.org/privatebrowsing;1"]
         .getService(Ci.nsIPrivateBrowsingService);

function runTests() {
  // prepare the grid
  yield setLinks("0,1,2,3,4,5,6,7,8,9");
  ok(!pb.privateBrowsingEnabled, "private browsing is disabled");

  yield addNewTabPageTab();
  pinCell(0);
  checkGrid("0p,1,2,3,4,5,6,7,8");

  // enter private browsing mode
  yield togglePrivateBrowsing();
  ok(pb.privateBrowsingEnabled, "private browsing is enabled");

  yield addNewTabPageTab();
  checkGrid("0p,1,2,3,4,5,6,7,8");

  // modify the grid while we're in pb mode
  yield blockCell(1);
  checkGrid("0p,2,3,4,5,6,7,8");

  yield unpinCell(0);
  checkGrid("0,2,3,4,5,6,7,8");

  // exit private browsing mode
  yield togglePrivateBrowsing();
  ok(!pb.privateBrowsingEnabled, "private browsing is disabled");

  // check that the grid is the same as before entering pb mode
  yield addNewTabPageTab();
  checkGrid("0,2,3,4,5,6,7,8")
}

function togglePrivateBrowsing() {
  let topic = "private-browsing-transition-complete";

  Services.obs.addObserver(function observe() {
    Services.obs.removeObserver(observe, topic);
    executeSoon(TestRunner.next);
  }, topic, false);

  pb.privateBrowsingEnabled = !pb.privateBrowsingEnabled;
}
