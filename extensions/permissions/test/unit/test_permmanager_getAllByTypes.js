/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function check_enumerator(permissionTypes, expectedPermissions) {
  const permissions = Services.perms.getAllByTypes(permissionTypes);

  Assert.equal(
    permissions.length,
    expectedPermissions.length,
    `getAllByTypes returned the expected number of permissions for ${JSON.stringify(
      permissionTypes
    )}`
  );

  for (const perm of permissions) {
    Assert.ok(perm != null);

    // For some reason, the order in which we get the permissions doesn't seem to be
    // stable when running the test with --verify. As a result, we need to retrieve the
    // expected permission for the origin and type.
    const expectedPermission = expectedPermissions.find(
      ([expectedPrincipal, expectedType]) =>
        perm.principal.equals(expectedPrincipal) && perm.type === expectedType
    );

    Assert.ok(expectedPermission !== null, "Found the expected permission");
    Assert.equal(perm.capability, expectedPermission[2]);
    Assert.equal(perm.expireType, Services.perms.EXPIRE_NEVER);
  }
}

function run_test() {
  let pm = Services.perms;

  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "http://example.com"
  );
  let subPrincipal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "http://sub.example.com"
  );

  const PERM_TYPE_1 = "test/getallbytypes_1";
  const PERM_TYPE_2 = "test/getallbytypes_2";

  info("check default state");
  check_enumerator([], []);
  check_enumerator([PERM_TYPE_1], []);
  check_enumerator([PERM_TYPE_2], []);
  check_enumerator([PERM_TYPE_1, PERM_TYPE_2], []);

  info("check that expected permissions are retrieved");
  pm.addFromPrincipal(principal, PERM_TYPE_1, pm.ALLOW_ACTION);
  pm.addFromPrincipal(
    subPrincipal,
    "other-test/getallbytypes_1",
    pm.PROMPT_ACTION
  );
  check_enumerator([PERM_TYPE_1], [[principal, PERM_TYPE_1, pm.ALLOW_ACTION]]);
  check_enumerator(
    [PERM_TYPE_1, PERM_TYPE_2],
    [[principal, PERM_TYPE_1, pm.ALLOW_ACTION]]
  );
  check_enumerator([], []);
  check_enumerator([PERM_TYPE_2], []);

  pm.addFromPrincipal(subPrincipal, PERM_TYPE_1, pm.PROMPT_ACTION);
  check_enumerator(
    [PERM_TYPE_1],
    [
      [subPrincipal, PERM_TYPE_1, pm.PROMPT_ACTION],
      [principal, PERM_TYPE_1, pm.ALLOW_ACTION],
    ]
  );
  check_enumerator(
    [PERM_TYPE_1, PERM_TYPE_2],
    [
      [subPrincipal, PERM_TYPE_1, pm.PROMPT_ACTION],
      [principal, PERM_TYPE_1, pm.ALLOW_ACTION],
    ]
  );
  check_enumerator([], []);
  check_enumerator([PERM_TYPE_2], []);

  pm.addFromPrincipal(principal, PERM_TYPE_2, pm.PROMPT_ACTION);
  check_enumerator(
    [PERM_TYPE_1, PERM_TYPE_2],
    [
      [subPrincipal, PERM_TYPE_1, pm.PROMPT_ACTION],
      [principal, PERM_TYPE_1, pm.ALLOW_ACTION],
      [principal, PERM_TYPE_2, pm.PROMPT_ACTION],
    ]
  );
  check_enumerator([], []);
  check_enumerator([PERM_TYPE_2], [[principal, PERM_TYPE_2, pm.PROMPT_ACTION]]);

  info("check that UNKNOWN_ACTION permissions are ignored");
  pm.addFromPrincipal(subPrincipal, PERM_TYPE_2, pm.UNKNOWN_ACTION);
  check_enumerator([PERM_TYPE_2], [[principal, PERM_TYPE_2, pm.PROMPT_ACTION]]);

  info("check that permission updates are reflected");
  pm.addFromPrincipal(subPrincipal, PERM_TYPE_2, pm.PROMPT_ACTION);
  check_enumerator(
    [PERM_TYPE_2],
    [
      [subPrincipal, PERM_TYPE_2, pm.PROMPT_ACTION],
      [principal, PERM_TYPE_2, pm.PROMPT_ACTION],
    ]
  );

  info("check that permission removals are reflected");
  pm.removeFromPrincipal(principal, PERM_TYPE_1);
  check_enumerator(
    [PERM_TYPE_1],
    [[subPrincipal, PERM_TYPE_1, pm.PROMPT_ACTION]]
  );

  pm.removeAll();
  check_enumerator([], []);
  check_enumerator([PERM_TYPE_1], []);
  check_enumerator([PERM_TYPE_2], []);
  check_enumerator([PERM_TYPE_1, PERM_TYPE_2], []);
}
