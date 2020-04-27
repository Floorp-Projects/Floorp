/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that permissions with specific expiry times behave as expected.
var test_generator = do_run_test();

function run_test() {
  do_test_pending();
  test_generator.next();
}

function continue_test() {
  do_run_generator(test_generator);
}

function* do_run_test() {
  let pm = Services.perms;
  let permURI = NetUtil.newURI("http://example.com");
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    permURI,
    {}
  );

  let now = Number(Date.now());

  // add a permission with *now* expiration
  pm.addFromPrincipal(
    principal,
    "test/expiration-perm-exp",
    1,
    pm.EXPIRE_TIME,
    now
  );
  pm.addFromPrincipal(
    principal,
    "test/expiration-session-exp",
    1,
    pm.EXPIRE_SESSION,
    now
  );

  // add a permission with future expiration (100 milliseconds)
  pm.addFromPrincipal(
    principal,
    "test/expiration-perm-exp2",
    1,
    pm.EXPIRE_TIME,
    now + 100
  );
  pm.addFromPrincipal(
    principal,
    "test/expiration-session-exp2",
    1,
    pm.EXPIRE_SESSION,
    now + 100
  );

  // add a permission with future expiration (1000 seconds)
  pm.addFromPrincipal(
    principal,
    "test/expiration-perm-exp3",
    1,
    pm.EXPIRE_TIME,
    now + 1e6
  );
  pm.addFromPrincipal(
    principal,
    "test/expiration-session-exp3",
    1,
    pm.EXPIRE_SESSION,
    now + 1e6
  );

  // add a permission without expiration
  pm.addFromPrincipal(
    principal,
    "test/expiration-perm-nexp",
    1,
    pm.EXPIRE_NEVER,
    0
  );

  // check that the second two haven't expired yet
  Assert.equal(
    1,
    pm.testPermissionFromPrincipal(principal, "test/expiration-perm-exp3")
  );
  Assert.equal(
    1,
    pm.testPermissionFromPrincipal(principal, "test/expiration-session-exp3")
  );
  Assert.equal(
    1,
    pm.testPermissionFromPrincipal(principal, "test/expiration-perm-nexp")
  );
  Assert.equal(1, pm.getAllWithTypePrefix("test/expiration-perm-exp3").length);
  Assert.equal(
    1,
    pm.getAllWithTypePrefix("test/expiration-session-exp3").length
  );
  Assert.equal(1, pm.getAllWithTypePrefix("test/expiration-perm-nexp").length);
  Assert.equal(5, pm.getAllForPrincipal(principal).length);

  // ... and the first one has
  do_timeout(10, continue_test);
  yield;
  Assert.equal(
    0,
    pm.testPermissionFromPrincipal(principal, "test/expiration-perm-exp")
  );
  Assert.equal(
    0,
    pm.testPermissionFromPrincipal(principal, "test/expiration-session-exp")
  );

  // ... and that the short-term one will
  do_timeout(200, continue_test);
  yield;
  Assert.equal(
    0,
    pm.testPermissionFromPrincipal(principal, "test/expiration-perm-exp2")
  );
  Assert.equal(
    0,
    pm.testPermissionFromPrincipal(principal, "test/expiration-session-exp2")
  );
  Assert.equal(0, pm.getAllWithTypePrefix("test/expiration-perm-exp2").length);
  Assert.equal(
    0,
    pm.getAllWithTypePrefix("test/expiration-session-exp2").length
  );

  Assert.equal(3, pm.getAllForPrincipal(principal).length);

  // Check that .getPermission returns a matching result
  Assert.equal(
    null,
    pm.getPermissionObject(principal, "test/expiration-perm-exp", false)
  );
  Assert.equal(
    null,
    pm.getPermissionObject(principal, "test/expiration-session-exp", false)
  );
  Assert.equal(
    null,
    pm.getPermissionObject(principal, "test/expiration-perm-exp2", false)
  );
  Assert.equal(
    null,
    pm.getPermissionObject(principal, "test/expiration-session-exp2", false)
  );

  // Add a persistent permission for private browsing
  let principalPB = Services.scriptSecurityManager.createContentPrincipal(
    permURI,
    { privateBrowsingId: 1 }
  );
  pm.addFromPrincipal(
    principalPB,
    "test/expiration-session-pb",
    pm.ALLOW_ACTION
  );

  // The permission should be set to session expiry
  let perm = pm.getPermissionObject(
    principalPB,
    "test/expiration-session-pb",
    true
  );
  Assert.equal(perm.expireType, pm.EXPIRE_SESSION);

  do_finish_generator_test(test_generator);
}
