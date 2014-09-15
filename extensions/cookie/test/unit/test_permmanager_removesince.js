/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that removing permissions since a specified time behaves as expected.

let test_generator = do_run_test();

function run_test() {
  do_test_pending();
  test_generator.next();
}

function continue_test()
{
  do_run_generator(test_generator);
}

function do_run_test() {
  // Set up a profile.
  let profile = do_get_profile();

  let pm = Services.perms;

  // to help with testing edge-cases, we will arrange for .removeAllSince to
  // remove *all* permissions from one principal and one permission from another.
  let permURI1 = NetUtil.newURI("http://example.com");
  let principal1 = Services.scriptSecurityManager.getNoAppCodebasePrincipal(permURI1);

  let permURI2 = NetUtil.newURI("http://example.org");
  let principal2 = Services.scriptSecurityManager.getNoAppCodebasePrincipal(permURI2);

  // add a permission now - this isn't going to be removed.
  pm.addFromPrincipal(principal1, "test/remove-since", 1);

  // sleep briefly, then record the time - we'll remove all since then.
  do_timeout(20, continue_test);
  yield;

  let since = Number(Date.now());

  // *sob* - on Windows at least, the now recorded by nsPermissionManager.cpp
  // might be a couple of ms *earlier* than what JS sees.  So another sleep
  // to ensure our |since| is greater than the time of the permissions we
  // are now adding.  Sadly this means we'll never be able to test when since
  // exactly equals the modTime, but there you go...
  do_timeout(20, continue_test);
  yield;

  // add another item - this second one should get nuked.
  pm.addFromPrincipal(principal1, "test/remove-since-2", 1);

  // add 2 items for the second principal - both will be removed.
  pm.addFromPrincipal(principal2, "test/remove-since", 1);
  pm.addFromPrincipal(principal2, "test/remove-since-2", 1);

  // do the removal.
  pm.removeAllSince(since);

  // principal1 - the first one should remain.
  do_check_eq(1, pm.testPermissionFromPrincipal(principal1, "test/remove-since"));
  // but the second should have been removed.
  do_check_eq(0, pm.testPermissionFromPrincipal(principal1, "test/remove-since-2"));

  // principal2 - both should have been removed.
  do_check_eq(0, pm.testPermissionFromPrincipal(principal2, "test/remove-since"));
  do_check_eq(0, pm.testPermissionFromPrincipal(principal2, "test/remove-since-2"));

  do_finish_generator_test(test_generator);
}
