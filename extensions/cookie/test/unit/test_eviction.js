/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let test_generator = do_run_test();

function run_test() {
  do_test_pending();
  test_generator.next();
}

function finish_test() {
  do_execute_soon(function() {
    test_generator.close();
    do_test_finished();
  });
}

function do_run_test()
{
  // Set up a profile.
  let profile = do_get_profile();

  // twiddle prefs to convenient values for this test
  Services.prefs.setIntPref("network.cookie.purgeAge", 1);
  Services.prefs.setIntPref("network.cookie.maxNumber", 1000);

  // eviction is performed based on two limits: when the total number of cookies
  // exceeds maxNumber + 10% (1100), and when cookies are older than purgeAge
  // (1 second). purging is done when both conditions are satisfied, and only
  // those cookies are purged.

  // we test the following cases of eviction:
  // 1) excess and age are satisfied, but only some of the excess are old enough
  // to be purged.
  force_eviction(1101, 50);

  // Fake a profile change, to ensure eviction affects the database correctly.
  do_close_profile(test_generator);
  yield;
  do_load_profile();

  do_check_true(check_remaining_cookies(1101, 50, 1051));

  // 2) excess and age are satisfied, and all of the excess are old enough
  // to be purged.
  force_eviction(1101, 100);
  do_close_profile(test_generator);
  yield;
  do_load_profile();
  do_check_true(check_remaining_cookies(1101, 100, 1001));

  // 3) excess and age are satisfied, and more than the excess are old enough
  // to be purged.
  force_eviction(1101, 500);
  do_close_profile(test_generator);
  yield;
  do_load_profile();
  do_check_true(check_remaining_cookies(1101, 500, 1001));

  // 4) excess but not age are satisfied.
  force_eviction(2000, 0);
  do_close_profile(test_generator);
  yield;
  do_load_profile();
  do_check_true(check_remaining_cookies(2000, 0, 2000));

  // 5) age but not excess are satisfied.
  force_eviction(1100, 200);
  do_close_profile(test_generator);
  yield;
  do_load_profile();
  do_check_true(check_remaining_cookies(1100, 200, 1100));

  finish_test();
}

// Set 'aNumberTotal' cookies, ensuring that the first 'aNumberOld' cookies
// will be measurably older than the rest.
function
force_eviction(aNumberTotal, aNumberOld)
{
  Services.cookiemgr.removeAll();
  var expiry = (Date.now() + 1e6) * 1000;

  var i;
  for (i = 0; i < aNumberTotal; ++i) {
    var host = "eviction." + i + ".tests";
    Services.cookiemgr.add(host, "", "test", "eviction", false, false, false,
      expiry);

    if (i == aNumberOld - 1) {
      // Sleep a while, to make sure the first batch of cookies is older than
      // the second (timer resolution varies on different platforms). This
      // delay must be measurably greater than the eviction age threshold.
      sleep(2000);
    }
  }
}

// Test that 'aNumberToExpect' cookies remain after purging is complete, and
// that the cookies that remain consist of the set expected given the number of
// of older and newer cookies -- eviction should occur by order of lastAccessed
// time, if both the limit on total cookies (maxNumber + 10%) and the purge age
// are exceeded.
function check_remaining_cookies(aNumberTotal, aNumberOld, aNumberToExpect) {
  var enumerator = Services.cookiemgr.enumerator;

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

  return i == aNumberToExpect;
}

// delay for a number of milliseconds
function sleep(delay)
{
  var start = Date.now();
  while (Date.now() < start + delay);
}

