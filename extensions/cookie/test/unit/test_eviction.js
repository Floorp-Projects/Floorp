/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test()
{
  var cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager2);
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  // twiddle prefs to convenient values for this test
  prefs.setIntPref("network.cookie.purgeAge", 1);
  prefs.setIntPref("network.cookie.maxNumber", 1000);

  // eviction is performed based on two limits: when the total number of cookies
  // exceeds maxNumber + 10% (1100), and when cookies are older than purgeAge
  // (1 second). purging is done when both conditions are satisfied, and only
  // those cookies are purged.

  // we test the following cases of eviction:
  // 1) excess and age are satisfied, but only some of the excess are old enough
  // to be purged.
  do_check_eq(testEviction(cm, 1101, 2, 50, 1051), 1051);

  // 2) excess and age are satisfied, and all of the excess are old enough
  // to be purged.
  do_check_eq(testEviction(cm, 1101, 2, 100, 1001), 1001);

  // 3) excess and age are satisfied, and more than the excess are old enough
  // to be purged.
  do_check_eq(testEviction(cm, 1101, 2, 500, 1001), 1001);

  // 4) excess but not age are satisfied.
  do_check_eq(testEviction(cm, 2000, 0, 0, 2000), 2000);

  // 5) age but not excess are satisfied.
  do_check_eq(testEviction(cm, 1100, 2, 200, 1100), 1100);

  cm.removeAll();

  // reset prefs to defaults
  prefs.setIntPref("network.cookie.purgeAge", 30 * 24 * 60 * 60);
  prefs.setIntPref("network.cookie.maxNumber", 2000);
}

// test that cookies are evicted by order of lastAccessed time, if both the limit
// on total cookies (maxNumber + 10%) and the purge age are exceeded
function
testEviction(aCM, aNumberTotal, aSleepDuration, aNumberOld, aNumberToExpect)
{
  aCM.removeAll();
  var expiry = (Date.now() + 1e6) * 1000;

  var i;
  for (i = 0; i < aNumberTotal; ++i) {
    var host = "eviction." + i + ".tests";
    aCM.add(host, "", "test", "eviction", false, false, false, expiry);

    if ((i == aNumberOld - 1) && aSleepDuration) {
      // sleep a while, to make sure the first batch of cookies is older than
      // the second (timer resolution varies on different platforms).
      sleep(aSleepDuration * 1000);
    }
  }

  var enumerator = aCM.enumerator;

  i = 0;
  while (enumerator.hasMoreElements()) {
    var cookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);
    ++i;

    if (aNumberTotal != aNumberToExpect) {
      // make sure the cookie is one of the batch we expect was purged.
      var hostNumber = new Number(cookie.rawHost.split(".")[1]);
      if (hostNumber < (aNumberOld - aNumberToExpect)) break;
    }
  }

  return i;
}

// delay for a number of milliseconds
function sleep(delay)
{
  var start = Date.now();
  while (Date.now() < start + delay);
}

