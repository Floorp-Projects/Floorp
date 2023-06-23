const TEST_SITE_URI = Services.io.newURI("http://example.com");
const TEST_FQDN_1_URI = Services.io.newURI("http://test1.example.com");
const TEST_FQDN_2_URI = Services.io.newURI("http://test2.example.com");
const TEST_OTHER_URI = Services.io.newURI("http://example.net");
const TEST_PERMISSION = "3rdPartyStorage^https://example.org";
const TEST_FRAME_PERMISSION = "3rdPartyFrameStorage^https://example.org";

async function do_test(permission) {
  let pm = Services.perms;

  let principal = Services.scriptSecurityManager.createContentPrincipal(
    TEST_SITE_URI,
    {}
  );

  let subdomain1Principal =
    Services.scriptSecurityManager.createContentPrincipal(TEST_FQDN_1_URI, {});

  let subdomain2Principal =
    Services.scriptSecurityManager.createContentPrincipal(TEST_FQDN_2_URI, {});

  let otherPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    TEST_OTHER_URI,
    {}
  );

  // Set test permission for site
  pm.addFromPrincipal(principal, permission, pm.ALLOW_ACTION);

  // Check normal site permission
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal, permission)
  );

  // Check subdomain permission
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(subdomain1Principal, permission)
  );

  // Check other site permission
  Assert.equal(
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(otherPrincipal, permission)
  );

  // Remove the permission from the site
  pm.removeFromPrincipal(principal, permission);
  Assert.equal(
    pm.testPermissionFromPrincipal(principal, permission),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(subdomain1Principal, permission),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );

  // Set test permission for subdomain
  pm.addFromPrincipal(subdomain1Principal, permission, pm.ALLOW_ACTION);

  // Check normal site permission
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal, permission)
  );

  // Check subdomain permission
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(subdomain1Principal, permission)
  );

  // Check other subdomain permission
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(subdomain2Principal, permission)
  );

  // Check other site permission
  Assert.equal(
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(otherPrincipal, permission)
  );

  // Check that subdomains include the site-scoped in the getAllForPrincipal
  let sitePerms = pm.getAllForPrincipal(principal, permission);
  let subdomain1Perms = pm.getAllForPrincipal(subdomain1Principal, permission);
  let subdomain2Perms = pm.getAllForPrincipal(subdomain2Principal, permission);
  let otherSitePerms = pm.getAllForPrincipal(otherPrincipal, permission);

  Assert.equal(sitePerms.length, 1);
  Assert.equal(subdomain1Perms.length, 1);
  Assert.equal(subdomain2Perms.length, 1);
  Assert.equal(otherSitePerms.length, 0);

  // Remove the permission from the subdomain
  pm.removeFromPrincipal(subdomain1Principal, permission);
  Assert.equal(
    pm.testPermissionFromPrincipal(principal, permission),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(subdomain1Principal, permission),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(subdomain2Principal, permission),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );
}

add_task(async function do3rdPartyStorageTest() {
  do_test(TEST_PERMISSION);
});

add_task(async function do3rdPartyFrameStorageTest() {
  do_test(TEST_FRAME_PERMISSION);
});
