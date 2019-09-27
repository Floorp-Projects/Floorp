/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function check_enumerator(principal, permissions) {
  let pm = Cc["@mozilla.org/permissionmanager;1"].getService(
    Ci.nsIPermissionManager
  );

  let enumerator = pm.getAllForPrincipal(principal);
  for ([type, capability] of permissions) {
    let perm = enumerator.getNext();
    Assert.ok(perm != null);
    Assert.equal(perm.type, type);
    Assert.equal(perm.capability, capability);
    Assert.equal(perm.expireType, pm.EXPIRE_NEVER);
  }
  Assert.ok(!enumerator.hasMoreElements());
}

function run_test() {
  let pm = Cc["@mozilla.org/permissionmanager;1"].getService(
    Ci.nsIPermissionManager
  );

  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "http://example.com"
  );
  let subPrincipal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "http://sub.example.com"
  );

  check_enumerator(principal, []);

  pm.addFromPrincipal(principal, "test/getallforuri", pm.ALLOW_ACTION);
  check_enumerator(principal, [["test/getallforuri", pm.ALLOW_ACTION]]);

  // check that uris are matched exactly
  check_enumerator(subPrincipal, []);

  pm.addFromPrincipal(subPrincipal, "test/getallforuri", pm.PROMPT_ACTION);
  pm.addFromPrincipal(subPrincipal, "test/getallforuri2", pm.DENY_ACTION);

  check_enumerator(subPrincipal, [
    ["test/getallforuri", pm.PROMPT_ACTION],
    ["test/getallforuri2", pm.DENY_ACTION],
  ]);

  // check that the original uri list has not changed
  check_enumerator(principal, [["test/getallforuri", pm.ALLOW_ACTION]]);

  // check that UNKNOWN_ACTION permissions are ignored
  pm.addFromPrincipal(principal, "test/getallforuri2", pm.UNKNOWN_ACTION);
  pm.addFromPrincipal(principal, "test/getallforuri3", pm.DENY_ACTION);

  check_enumerator(principal, [
    ["test/getallforuri", pm.ALLOW_ACTION],
    ["test/getallforuri3", pm.DENY_ACTION],
  ]);

  // check that permission updates are reflected
  pm.addFromPrincipal(principal, "test/getallforuri", pm.PROMPT_ACTION);

  check_enumerator(principal, [
    ["test/getallforuri", pm.PROMPT_ACTION],
    ["test/getallforuri3", pm.DENY_ACTION],
  ]);

  // check that permission removals are reflected
  pm.removeFromPrincipal(principal, "test/getallforuri");

  check_enumerator(principal, [["test/getallforuri3", pm.DENY_ACTION]]);

  pm.removeAll();
  check_enumerator(principal, []);
  check_enumerator(subPrincipal, []);
}
