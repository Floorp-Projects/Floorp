/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function getPrincipalFromURI(uri) {
  return Cc["@mozilla.org/scriptsecuritymanager;1"]
           .getService(Ci.nsIScriptSecurityManager)
           .getNoAppCodebasePrincipal(NetUtil.newURI(uri));
}

function getSystemPrincipal() {
  return Cc["@mozilla.org/scriptsecuritymanager;1"]
           .getService(Ci.nsIScriptSecurityManager)
           .getSystemPrincipal();
}

function run_test() {
  var pm = Cc["@mozilla.org/permissionmanager;1"].
           getService(Ci.nsIPermissionManager);

  do_check_null(pm.getPermissionObject(getSystemPrincipal(), "test/pobject", false));

  let principal = getPrincipalFromURI("http://example.com");
  let subPrincipal = getPrincipalFromURI("http://sub.example.com");
  let subSubPrincipal = getPrincipalFromURI("http://sub.sub.example.com");

  do_check_null(pm.getPermissionObject(principal, "test/pobject", false));
  do_check_null(pm.getPermissionObject(principal, "test/pobject", true));

  pm.addFromPrincipal(principal, "test/pobject", pm.ALLOW_ACTION);
  var rootPerm = pm.getPermissionObject(principal, "test/pobject", false);
  do_check_true(rootPerm != null);
  do_check_eq(rootPerm.principal.origin, "http://example.com");
  do_check_eq(rootPerm.type, "test/pobject");
  do_check_eq(rootPerm.capability, pm.ALLOW_ACTION);
  do_check_eq(rootPerm.expireType, pm.EXPIRE_NEVER);

  var rootPerm2 = pm.getPermissionObject(principal, "test/pobject", true);
  do_check_true(rootPerm != null);
  do_check_eq(rootPerm.principal.origin, "http://example.com");

  var subPerm = pm.getPermissionObject(subPrincipal, "test/pobject", true);
  do_check_null(subPerm);
  subPerm = pm.getPermissionObject(subPrincipal, "test/pobject", false);
  do_check_true(subPerm != null);
  do_check_eq(subPerm.principal.origin, "http://example.com");
  do_check_eq(subPerm.type, "test/pobject");
  do_check_eq(subPerm.capability, pm.ALLOW_ACTION);

  subPerm = pm.getPermissionObject(subSubPrincipal, "test/pobject", true);
  do_check_null(subPerm);
  subPerm = pm.getPermissionObject(subSubPrincipal, "test/pobject", false);
  do_check_true(subPerm != null);
  do_check_eq(subPerm.principal.origin, "http://example.com");

  pm.addFromPrincipal(principal, "test/pobject", pm.DENY_ACTION, pm.EXPIRE_SESSION);

  // make sure permission objects are not dynamic
  do_check_eq(rootPerm.capability, pm.ALLOW_ACTION);

  // but do update on change
  rootPerm = pm.getPermissionObject(principal, "test/pobject", true);
  do_check_eq(rootPerm.capability, pm.DENY_ACTION);
  do_check_eq(rootPerm.expireType, pm.EXPIRE_SESSION);

  subPerm = pm.getPermissionObject(subPrincipal, "test/pobject", false);
  do_check_eq(subPerm.principal.origin, "http://example.com");
  do_check_eq(subPerm.capability, pm.DENY_ACTION);
  do_check_eq(subPerm.expireType, pm.EXPIRE_SESSION);

  pm.addFromPrincipal(subPrincipal, "test/pobject", pm.PROMPT_ACTION);
  rootPerm = pm.getPermissionObject(principal, "test/pobject", true);
  do_check_eq(rootPerm.principal.origin, "http://example.com");
  do_check_eq(rootPerm.capability, pm.DENY_ACTION);

  subPerm = pm.getPermissionObject(subPrincipal, "test/pobject", true);
  do_check_eq(subPerm.principal.origin, "http://sub.example.com");
  do_check_eq(subPerm.capability, pm.PROMPT_ACTION);

  subPerm = pm.getPermissionObject(subPrincipal, "test/pobject", false);
  do_check_eq(subPerm.principal.origin, "http://sub.example.com");
  do_check_eq(subPerm.capability, pm.PROMPT_ACTION);

  subPerm = pm.getPermissionObject(subSubPrincipal, "test/pobject", true);
  do_check_null(subPerm);

  subPerm = pm.getPermissionObject(subSubPrincipal, "test/pobject", false);
  do_check_eq(subPerm.principal.origin, "http://sub.example.com");
  do_check_eq(subPerm.capability, pm.PROMPT_ACTION);

  pm.removeFromPrincipal(principal, "test/pobject");

  rootPerm = pm.getPermissionObject(principal, "test/pobject", true);
  do_check_null(rootPerm);
}
