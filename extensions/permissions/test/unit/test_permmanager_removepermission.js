/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
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

  pm.addFromPrincipal(principal, "apple", 0);
  pm.addFromPrincipal(principal, "apple", 3);
  pm.addFromPrincipal(principal, "pear", 3);
  pm.addFromPrincipal(principal, "pear", 1);
  pm.addFromPrincipal(principal, "cucumber", 1);
  pm.addFromPrincipal(principal, "cucumber", 1);
  pm.addFromPrincipal(principal, "cucumber", 1);

  pm.addFromPrincipal(principal2, "apple", 2);
  pm.addFromPrincipal(principal2, "pear", 0);
  pm.addFromPrincipal(principal2, "pear", 2);

  // Make sure that removePermission doesn't remove more than one permission each time
  Assert.equal(pm.all.length, 5);

  remove_one_by_type("apple");
  Assert.equal(pm.all.length, 4);

  remove_one_by_type("apple");
  Assert.equal(pm.all.length, 3);

  remove_one_by_type("pear");
  Assert.equal(pm.all.length, 2);

  remove_one_by_type("cucumber");
  Assert.equal(pm.all.length, 1);

  remove_one_by_type("pear");
  Assert.equal(pm.all.length, 0);

  function remove_one_by_type(type) {
    for (let perm of pm.all) {
      if (perm.type == type) {
        pm.removePermission(perm);
        break;
      }
    }
  }
}
