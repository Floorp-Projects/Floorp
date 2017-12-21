/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function getPrincipalFromURI(aURI) {
  let ssm = Cc["@mozilla.org/scriptsecuritymanager;1"]
              .getService(Ci.nsIScriptSecurityManager);
  let uri = NetUtil.newURI(aURI);
  return ssm.createCodebasePrincipal(uri, {});
}

function getSystemPrincipal() {
  return Cc["@mozilla.org/scriptsecuritymanager;1"]
           .getService(Ci.nsIScriptSecurityManager)
           .getSystemPrincipal();
}

function run_test() {
  var pm = Cc["@mozilla.org/permissionmanager;1"].
           getService(Ci.nsIPermissionManager);

  Assert.equal(null, pm.getPermissionObject(getSystemPrincipal(), "test/pobject", false));

  let principal = getPrincipalFromURI("http://example.com");
  let subPrincipal = getPrincipalFromURI("http://sub.example.com");
  let subSubPrincipal = getPrincipalFromURI("http://sub.sub.example.com");

  Assert.equal(null, pm.getPermissionObject(principal, "test/pobject", false));
  Assert.equal(null, pm.getPermissionObject(principal, "test/pobject", true));

  pm.addFromPrincipal(principal, "test/pobject", pm.ALLOW_ACTION);
  var rootPerm = pm.getPermissionObject(principal, "test/pobject", false);
  Assert.ok(rootPerm != null);
  Assert.equal(rootPerm.principal.origin, "http://example.com");
  Assert.equal(rootPerm.type, "test/pobject");
  Assert.equal(rootPerm.capability, pm.ALLOW_ACTION);
  Assert.equal(rootPerm.expireType, pm.EXPIRE_NEVER);

  var rootPerm2 = pm.getPermissionObject(principal, "test/pobject", true);
  Assert.ok(rootPerm != null);
  Assert.equal(rootPerm.principal.origin, "http://example.com");

  var subPerm = pm.getPermissionObject(subPrincipal, "test/pobject", true);
  Assert.equal(null, subPerm);
  subPerm = pm.getPermissionObject(subPrincipal, "test/pobject", false);
  Assert.ok(subPerm != null);
  Assert.equal(subPerm.principal.origin, "http://example.com");
  Assert.equal(subPerm.type, "test/pobject");
  Assert.equal(subPerm.capability, pm.ALLOW_ACTION);

  subPerm = pm.getPermissionObject(subSubPrincipal, "test/pobject", true);
  Assert.equal(null, subPerm);
  subPerm = pm.getPermissionObject(subSubPrincipal, "test/pobject", false);
  Assert.ok(subPerm != null);
  Assert.equal(subPerm.principal.origin, "http://example.com");

  pm.addFromPrincipal(principal, "test/pobject", pm.DENY_ACTION, pm.EXPIRE_SESSION);

  // make sure permission objects are not dynamic
  Assert.equal(rootPerm.capability, pm.ALLOW_ACTION);

  // but do update on change
  rootPerm = pm.getPermissionObject(principal, "test/pobject", true);
  Assert.equal(rootPerm.capability, pm.DENY_ACTION);
  Assert.equal(rootPerm.expireType, pm.EXPIRE_SESSION);

  subPerm = pm.getPermissionObject(subPrincipal, "test/pobject", false);
  Assert.equal(subPerm.principal.origin, "http://example.com");
  Assert.equal(subPerm.capability, pm.DENY_ACTION);
  Assert.equal(subPerm.expireType, pm.EXPIRE_SESSION);

  pm.addFromPrincipal(subPrincipal, "test/pobject", pm.PROMPT_ACTION);
  rootPerm = pm.getPermissionObject(principal, "test/pobject", true);
  Assert.equal(rootPerm.principal.origin, "http://example.com");
  Assert.equal(rootPerm.capability, pm.DENY_ACTION);

  subPerm = pm.getPermissionObject(subPrincipal, "test/pobject", true);
  Assert.equal(subPerm.principal.origin, "http://sub.example.com");
  Assert.equal(subPerm.capability, pm.PROMPT_ACTION);

  subPerm = pm.getPermissionObject(subPrincipal, "test/pobject", false);
  Assert.equal(subPerm.principal.origin, "http://sub.example.com");
  Assert.equal(subPerm.capability, pm.PROMPT_ACTION);

  subPerm = pm.getPermissionObject(subSubPrincipal, "test/pobject", true);
  Assert.equal(null, subPerm);

  subPerm = pm.getPermissionObject(subSubPrincipal, "test/pobject", false);
  Assert.equal(subPerm.principal.origin, "http://sub.example.com");
  Assert.equal(subPerm.capability, pm.PROMPT_ACTION);

  pm.removeFromPrincipal(principal, "test/pobject");

  rootPerm = pm.getPermissionObject(principal, "test/pobject", true);
  Assert.equal(null, rootPerm);
}
