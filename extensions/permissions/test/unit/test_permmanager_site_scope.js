const TEST_SITE_URI = Services.io.newURI("http://example.com");
const TEST_FQDN_1_URI = Services.io.newURI("http://test1.example.com");
const TEST_FQDN_2_URI = Services.io.newURI("http://test2.example.com");
const TEST_OTHER_URI = Services.io.newURI("http://example.net");
const TEST_PERMISSION = "3rdPartyStorage^https://example.org";

add_task(async function do_test() {
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
  pm.addFromPrincipal(principal, TEST_PERMISSION, pm.ALLOW_ACTION);

  // Check normal site permission
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal, TEST_PERMISSION)
  );

  // Check subdomain permission
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(subdomain1Principal, TEST_PERMISSION)
  );

  // Check other site permission
  Assert.equal(
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(otherPrincipal, TEST_PERMISSION)
  );

  // Remove the permission from the site
  pm.removeFromPrincipal(principal, TEST_PERMISSION);
  Assert.equal(
    pm.testPermissionFromPrincipal(principal, TEST_PERMISSION),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(subdomain1Principal, TEST_PERMISSION),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );

  // Set test permission for subdomain
  pm.addFromPrincipal(subdomain1Principal, TEST_PERMISSION, pm.ALLOW_ACTION);

  // Check normal site permission
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal, TEST_PERMISSION)
  );

  // Check subdomain permission
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(subdomain1Principal, TEST_PERMISSION)
  );

  // Check other subdomain permission
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(subdomain2Principal, TEST_PERMISSION)
  );

  // Check other site permission
  Assert.equal(
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(otherPrincipal, TEST_PERMISSION)
  );

  // Check that subdomains include the site-scoped in the getAllForPrincipal
  let sitePerms = pm.getAllForPrincipal(principal, TEST_PERMISSION);
  let subdomain1Perms = pm.getAllForPrincipal(
    subdomain1Principal,
    TEST_PERMISSION
  );
  let subdomain2Perms = pm.getAllForPrincipal(
    subdomain2Principal,
    TEST_PERMISSION
  );
  let otherSitePerms = pm.getAllForPrincipal(otherPrincipal, TEST_PERMISSION);

  Assert.equal(sitePerms.length, 1);
  Assert.equal(subdomain1Perms.length, 1);
  Assert.equal(subdomain2Perms.length, 1);
  Assert.equal(otherSitePerms.length, 0);

  // Remove the permission from the subdomain
  pm.removeFromPrincipal(subdomain1Principal, TEST_PERMISSION);
  Assert.equal(
    pm.testPermissionFromPrincipal(principal, TEST_PERMISSION),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(subdomain1Principal, TEST_PERMISSION),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(subdomain2Principal, TEST_PERMISSION),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );
});
