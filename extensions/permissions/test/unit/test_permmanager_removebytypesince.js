/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test() {
  Services.prefs.setCharPref("permissions.manager.defaultsUrl", "");

  // initialize the permission manager service
  let pm = Services.perms;

  Assert.equal(pm.all.length, 0);

  // add some permissions
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "http://amazon.com:8080"
  );
  let principal2 = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "http://google.com:2048"
  );
  let principal3 = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "https://google.com"
  );

  pm.addFromPrincipal(principal, "apple", 3);
  pm.addFromPrincipal(principal, "pear", 1);
  pm.addFromPrincipal(principal, "cucumber", 1);

  // sleep briefly, then record the time - we'll remove some permissions since then.
  await new Promise(resolve => do_timeout(20, resolve));

  let since = Date.now();

  // *sob* - on Windows at least, the now recorded by nsPermissionManager.cpp
  // might be a couple of ms *earlier* than what JS sees.  So another sleep
  // to ensure our |since| is greater than the time of the permissions we
  // are now adding.  Sadly this means we'll never be able to test when since
  // exactly equals the modTime, but there you go...
  await new Promise(resolve => do_timeout(20, resolve));

  pm.addFromPrincipal(principal2, "apple", 2);
  pm.addFromPrincipal(principal2, "pear", 2);

  pm.addFromPrincipal(principal3, "cucumber", 3);
  pm.addFromPrincipal(principal3, "apple", 1);

  Assert.equal(pm.all.length, 7);

  pm.removeByTypeSince("apple", since);

  Assert.equal(pm.all.length, 5);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "pear"), 1);
  Assert.equal(pm.testPermissionFromPrincipal(principal2, "pear"), 2);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "apple"), 3);
  Assert.equal(pm.testPermissionFromPrincipal(principal2, "apple"), 0);
  Assert.equal(pm.testPermissionFromPrincipal(principal3, "apple"), 0);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "cucumber"), 1);
  Assert.equal(pm.testPermissionFromPrincipal(principal3, "cucumber"), 3);

  pm.removeByTypeSince("cucumber", since);
  Assert.equal(pm.all.length, 4);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "pear"), 1);
  Assert.equal(pm.testPermissionFromPrincipal(principal2, "pear"), 2);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "apple"), 3);
  Assert.equal(pm.testPermissionFromPrincipal(principal2, "apple"), 0);
  Assert.equal(pm.testPermissionFromPrincipal(principal3, "apple"), 0);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "cucumber"), 1);
  Assert.equal(pm.testPermissionFromPrincipal(principal3, "cucumber"), 0);

  pm.removeByTypeSince("pear", since);
  Assert.equal(pm.all.length, 3);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "pear"), 1);
  Assert.equal(pm.testPermissionFromPrincipal(principal2, "pear"), 0);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "apple"), 3);
  Assert.equal(pm.testPermissionFromPrincipal(principal2, "apple"), 0);
  Assert.equal(pm.testPermissionFromPrincipal(principal3, "apple"), 0);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "cucumber"), 1);
  Assert.equal(pm.testPermissionFromPrincipal(principal3, "cucumber"), 0);
});
