/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that permissions with specific expiry times behave as expected.

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

  let pm = Services.permissions;
  let permURI = NetUtil.newURI("http://example.com");
  let principal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(permURI);

  let now = Number(Date.now());

  // add a permission with *now* expiration
  pm.addFromPrincipal(principal, "test/expiration-perm-exp", 1, pm.EXPIRE_TIME, now);
  pm.addFromPrincipal(principal, "test/expiration-session-exp", 1, pm.EXPIRE_SESSION, now);

  // add a permission with future expiration (100 milliseconds)
  pm.addFromPrincipal(principal, "test/expiration-perm-exp2", 1, pm.EXPIRE_TIME, now + 100);
  pm.addFromPrincipal(principal, "test/expiration-session-exp2", 1, pm.EXPIRE_SESSION, now + 100);

  // add a permission with future expiration (1000 seconds)
  pm.addFromPrincipal(principal, "test/expiration-perm-exp3", 1, pm.EXPIRE_TIME, now + 1e6);
  pm.addFromPrincipal(principal, "test/expiration-session-exp3", 1, pm.EXPIRE_SESSION, now + 1e6);

  // add a permission without expiration
  pm.addFromPrincipal(principal, "test/expiration-perm-nexp", 1, pm.EXPIRE_NEVER, 0);

  // check that the second two haven't expired yet
  do_check_eq(1, pm.testPermissionFromPrincipal(principal, "test/expiration-perm-exp3"));
  do_check_eq(1, pm.testPermissionFromPrincipal(principal, "test/expiration-session-exp3"));
  do_check_eq(1, pm.testPermissionFromPrincipal(principal, "test/expiration-perm-nexp"));

  // ... and the first one has
  do_timeout(10, continue_test);
  yield;
  do_check_eq(0, pm.testPermissionFromPrincipal(principal, "test/expiration-perm-exp"));
  do_check_eq(0, pm.testPermissionFromPrincipal(principal, "test/expiration-session-exp"));

  // ... and that the short-term one will
  do_timeout(200, continue_test);
  yield;
  do_check_eq(0, pm.testPermissionFromPrincipal(principal, "test/expiration-perm-exp2"));
  do_check_eq(0, pm.testPermissionFromPrincipal(principal, "test/expiration-session-exp2"));

  // Check that .getPermission returns a matching result
  do_check_null(pm.getPermissionObject(principal, "test/expiration-perm-exp", false));
  do_check_null(pm.getPermissionObject(principal, "test/expiration-session-exp", false));
  do_check_null(pm.getPermissionObject(principal, "test/expiration-perm-exp2", false));
  do_check_null(pm.getPermissionObject(principal, "test/expiration-session-exp2", false));

  do_finish_generator_test(test_generator);
}

