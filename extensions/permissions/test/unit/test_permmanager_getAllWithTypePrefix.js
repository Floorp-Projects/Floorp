/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function check_enumerator(prefix, permissions) {
  let pm = Services.perms;

  let array = pm.getAllWithTypePrefix(prefix);
  for (let [principal, type, capability] of permissions) {
    let perm = array.shift();
    Assert.ok(perm != null);
    Assert.ok(perm.principal.equals(principal));
    Assert.equal(perm.type, type);
    Assert.equal(perm.capability, capability);
    Assert.equal(perm.expireType, pm.EXPIRE_NEVER);
  }
  Assert.equal(array.length, 0);
}

function run_test() {
  let pm = Services.perms;

  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "http://example.com"
    );
  let subPrincipal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "http://sub.example.com"
    );

  check_enumerator("test/", []);

  pm.addFromPrincipal(principal, "test/getallwithtypeprefix", pm.ALLOW_ACTION);
  pm.addFromPrincipal(
    subPrincipal,
    "other-test/getallwithtypeprefix",
    pm.PROMPT_ACTION
  );
  check_enumerator("test/", [
    [principal, "test/getallwithtypeprefix", pm.ALLOW_ACTION],
  ]);

  pm.addFromPrincipal(
    subPrincipal,
    "test/getallwithtypeprefix",
    pm.PROMPT_ACTION
  );
  check_enumerator("test/", [
    [subPrincipal, "test/getallwithtypeprefix", pm.PROMPT_ACTION],
    [principal, "test/getallwithtypeprefix", pm.ALLOW_ACTION],
  ]);

  check_enumerator("test/getallwithtypeprefix", [
    [subPrincipal, "test/getallwithtypeprefix", pm.PROMPT_ACTION],
    [principal, "test/getallwithtypeprefix", pm.ALLOW_ACTION],
  ]);

  // check that UNKNOWN_ACTION permissions are ignored
  pm.addFromPrincipal(
    principal,
    "test/getallwithtypeprefix2",
    pm.UNKNOWN_ACTION
  );
  check_enumerator("test/", [
    [subPrincipal, "test/getallwithtypeprefix", pm.PROMPT_ACTION],
    [principal, "test/getallwithtypeprefix", pm.ALLOW_ACTION],
  ]);

  // check that permission updates are reflected
  pm.addFromPrincipal(principal, "test/getallwithtypeprefix", pm.PROMPT_ACTION);
  check_enumerator("test/", [
    [subPrincipal, "test/getallwithtypeprefix", pm.PROMPT_ACTION],
    [principal, "test/getallwithtypeprefix", pm.PROMPT_ACTION],
  ]);

  // check that permission removals are reflected
  pm.removeFromPrincipal(principal, "test/getallwithtypeprefix");
  check_enumerator("test/", [
    [subPrincipal, "test/getallwithtypeprefix", pm.PROMPT_ACTION],
  ]);

  pm.removeAll();
  check_enumerator("test/", []);
}
