/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function check_enumerator(prefix, from, permissions) {
  let pm = Services.perms;

  let array = pm.getAllByTypeSince(prefix, from);
  Assert.equal(array.length, permissions.length);
  for (let [principal, type, capability] of permissions) {
    let perm = array.find(p => p.principal.equals(principal));
    Assert.ok(perm != null);
    Assert.equal(perm.type, type);
    Assert.equal(perm.capability, capability);
    Assert.equal(perm.expireType, pm.EXPIRE_NEVER);
  }
}

add_task(async function test() {
  let pm = Services.perms;

  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "http://example.com"
    );
  let subPrincipal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "http://sub.example.com"
    );

  check_enumerator("test/", 0, []);

  pm.addFromPrincipal(principal, "test/getAllByTypeSince", pm.ALLOW_ACTION);

  // These shouldn't show up anywhere, the name doesn't match.
  pm.addFromPrincipal(
    subPrincipal,
    "other-test/getAllByTypeSince",
    pm.PROMPT_ACTION
  );
  pm.addFromPrincipal(principal, "test/getAllByTypeSince1", pm.PROMPT_ACTION);

  check_enumerator("test/getAllByTypeSince", 0, [
    [principal, "test/getAllByTypeSince", pm.ALLOW_ACTION],
  ]);

  // Add some time in between taking the snapshot of the timestamp
  // to avoid flakyness.
  await new Promise(c => do_timeout(100, c));
  let timestamp = Date.now();
  await new Promise(c => do_timeout(100, c));

  pm.addFromPrincipal(subPrincipal, "test/getAllByTypeSince", pm.DENY_ACTION);

  check_enumerator("test/getAllByTypeSince", 0, [
    [subPrincipal, "test/getAllByTypeSince", pm.DENY_ACTION],
    [principal, "test/getAllByTypeSince", pm.ALLOW_ACTION],
  ]);

  check_enumerator("test/getAllByTypeSince", timestamp, [
    [subPrincipal, "test/getAllByTypeSince", pm.DENY_ACTION],
  ]);

  // check that UNKNOWN_ACTION permissions are ignored
  pm.addFromPrincipal(
    subPrincipal,
    "test/getAllByTypeSince",
    pm.UNKNOWN_ACTION
  );

  check_enumerator("test/getAllByTypeSince", 0, [
    [principal, "test/getAllByTypeSince", pm.ALLOW_ACTION],
  ]);

  // check that permission removals are reflected
  pm.removeFromPrincipal(principal, "test/getAllByTypeSince");
  check_enumerator("test/", 0, []);

  pm.removeAll();
});
