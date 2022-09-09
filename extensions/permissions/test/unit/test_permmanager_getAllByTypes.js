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

  permissions.forEach((perm, i) => {
    info(`Checking permission #${i}`);
    const [
      expectedPrincipal,
      expectedType,
      expectedCapability,
    ] = expectedPermissions[i];

    Assert.ok(perm != null);
    Assert.ok(perm.principal.equals(expectedPrincipal));
    Assert.equal(perm.type, expectedType);
    Assert.equal(perm.capability, expectedCapability);
    Assert.equal(perm.expireType, Services.perms.EXPIRE_NEVER);
  });
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
