/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  Services.prefs.setCharPref("permissions.manager.defaultsUrl", "");

  // initialize the permission manager service
  let pm = Services.perms;

  Assert.equal(pm.all.length, 0);

  // add some permissions
  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "http://amazon.com:8080"
    );
  let principal2 =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "http://google.com:2048"
    );
  let principal3 =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "https://google.com"
    );

  pm.addFromPrincipal(principal, "apple", 3);
  pm.addFromPrincipal(principal, "pear", 1);
  pm.addFromPrincipal(principal, "cucumber", 1);

  pm.addFromPrincipal(principal2, "apple", 2);
  pm.addFromPrincipal(principal2, "pear", 2);

  pm.addFromPrincipal(principal3, "cucumber", 3);
  pm.addFromPrincipal(principal3, "apple", 1);

  Assert.equal(pm.all.length, 7);

  pm.removeByType("apple");
  Assert.equal(pm.all.length, 4);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "pear"), 1);
  Assert.equal(pm.testPermissionFromPrincipal(principal2, "pear"), 2);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "apple"), 0);
  Assert.equal(pm.testPermissionFromPrincipal(principal2, "apple"), 0);
  Assert.equal(pm.testPermissionFromPrincipal(principal3, "apple"), 0);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "cucumber"), 1);
  Assert.equal(pm.testPermissionFromPrincipal(principal3, "cucumber"), 3);

  pm.removeByType("cucumber");
  Assert.equal(pm.all.length, 2);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "pear"), 1);
  Assert.equal(pm.testPermissionFromPrincipal(principal2, "pear"), 2);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "apple"), 0);
  Assert.equal(pm.testPermissionFromPrincipal(principal2, "apple"), 0);
  Assert.equal(pm.testPermissionFromPrincipal(principal3, "apple"), 0);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "cucumber"), 0);
  Assert.equal(pm.testPermissionFromPrincipal(principal3, "cucumber"), 0);

  pm.removeByType("pear");
  Assert.equal(pm.all.length, 0);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "pear"), 0);
  Assert.equal(pm.testPermissionFromPrincipal(principal2, "pear"), 0);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "apple"), 0);
  Assert.equal(pm.testPermissionFromPrincipal(principal2, "apple"), 0);
  Assert.equal(pm.testPermissionFromPrincipal(principal3, "apple"), 0);

  Assert.equal(pm.testPermissionFromPrincipal(principal, "cucumber"), 0);
  Assert.equal(pm.testPermissionFromPrincipal(principal3, "cucumber"), 0);
}
