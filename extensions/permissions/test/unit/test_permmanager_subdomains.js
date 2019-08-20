/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function getPrincipalFromURI(aURI) {
  let ssm = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(
    Ci.nsIScriptSecurityManager
  );
  let uri = NetUtil.newURI(aURI);
  return ssm.createContentPrincipal(uri, {});
}

function run_test() {
  var pm = Cc["@mozilla.org/permissionmanager;1"].getService(
    Ci.nsIPermissionManager
  );

  // Adds a permission to a sub-domain. Checks if it is working.
  let sub1Principal = getPrincipalFromURI("http://sub1.example.com");
  pm.addFromPrincipal(sub1Principal, "test/subdomains", pm.ALLOW_ACTION, 0, 0);
  Assert.equal(
    pm.testPermissionFromPrincipal(sub1Principal, "test/subdomains"),
    pm.ALLOW_ACTION
  );

  // A sub-sub-domain should get the permission.
  let subsubPrincipal = getPrincipalFromURI("http://sub.sub1.example.com");
  Assert.equal(
    pm.testPermissionFromPrincipal(subsubPrincipal, "test/subdomains"),
    pm.ALLOW_ACTION
  );

  // Another sub-domain shouldn't get the permission.
  let sub2Principal = getPrincipalFromURI("http://sub2.example.com");
  Assert.equal(
    pm.testPermissionFromPrincipal(sub2Principal, "test/subdomains"),
    pm.UNKNOWN_ACTION
  );

  // Remove current permissions.
  pm.removeFromPrincipal(sub1Principal, "test/subdomains");
  Assert.equal(
    pm.testPermissionFromPrincipal(sub1Principal, "test/subdomains"),
    pm.UNKNOWN_ACTION
  );

  // Adding the permission to the main domain. Checks if it is working.
  let mainPrincipal = getPrincipalFromURI("http://example.com");
  pm.addFromPrincipal(mainPrincipal, "test/subdomains", pm.ALLOW_ACTION, 0, 0);
  Assert.equal(
    pm.testPermissionFromPrincipal(mainPrincipal, "test/subdomains"),
    pm.ALLOW_ACTION
  );

  // All sub-domains should have the permission now.
  Assert.equal(
    pm.testPermissionFromPrincipal(sub1Principal, "test/subdomains"),
    pm.ALLOW_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(sub2Principal, "test/subdomains"),
    pm.ALLOW_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(subsubPrincipal, "test/subdomains"),
    pm.ALLOW_ACTION
  );

  // Remove current permissions.
  pm.removeFromPrincipal(mainPrincipal, "test/subdomains");
  Assert.equal(
    pm.testPermissionFromPrincipal(mainPrincipal, "test/subdomains"),
    pm.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(sub1Principal, "test/subdomains"),
    pm.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(sub2Principal, "test/subdomains"),
    pm.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(subsubPrincipal, "test/subdomains"),
    pm.UNKNOWN_ACTION
  );

  // A sanity check that the previous implementation wasn't passing...
  let crazyPrincipal = getPrincipalFromURI("http://com");
  pm.addFromPrincipal(crazyPrincipal, "test/subdomains", pm.ALLOW_ACTION, 0, 0);
  Assert.equal(
    pm.testPermissionFromPrincipal(crazyPrincipal, "test/subdomains"),
    pm.ALLOW_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(mainPrincipal, "test/subdomains"),
    pm.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(sub1Principal, "test/subdomains"),
    pm.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(sub2Principal, "test/subdomains"),
    pm.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(subsubPrincipal, "test/subdomains"),
    pm.UNKNOWN_ACTION
  );
}
