/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function check_enumerator(principal, permissions) {
  let perms = Services.perms.getAllForPrincipal(principal);
  for (let [type, capability, expireType] of permissions) {
    let perm = perms.shift();
    Assert.ok(perm != null);
    Assert.equal(perm.type, type);
    Assert.equal(perm.capability, capability);
    Assert.equal(perm.expireType, expireType);
  }
  Assert.ok(!perms.length);
}

add_task(async function test() {
  Services.prefs.setCharPref("permissions.manager.defaultsUrl", "");

  // setup a profile directory
  do_get_profile();

  // We need to execute a pm method to be sure that the DB is fully
  // initialized.
  var pm = Services.perms;
  Assert.ok(pm.all.length === 0);

  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "http://example.com"
    );

  info("From session to persistent");
  pm.addFromPrincipal(
    principal,
    "test/foo",
    pm.ALLOW_ACTION,
    pm.EXPIRE_SESSION
  );

  check_enumerator(principal, [
    ["test/foo", pm.ALLOW_ACTION, pm.EXPIRE_SESSION],
  ]);

  pm.addFromPrincipal(principal, "test/foo", pm.ALLOW_ACTION, pm.EXPIRE_NEVER);

  check_enumerator(principal, [["test/foo", pm.ALLOW_ACTION, pm.EXPIRE_NEVER]]);

  // Let's reload the DB.
  Services.obs.notifyObservers(null, "testonly-reload-permissions-from-disk");

  Assert.ok(pm.all.length === 1);
  check_enumerator(principal, [["test/foo", pm.ALLOW_ACTION, pm.EXPIRE_NEVER]]);

  info("From persistent to session");
  pm.addFromPrincipal(
    principal,
    "test/foo",
    pm.ALLOW_ACTION,
    pm.EXPIRE_SESSION
  );

  check_enumerator(principal, [
    ["test/foo", pm.ALLOW_ACTION, pm.EXPIRE_SESSION],
  ]);

  // Let's reload the DB.
  Services.obs.notifyObservers(null, "testonly-reload-permissions-from-disk");
  Assert.ok(pm.all.length === 0);

  info("From persistent to persistent");
  pm.addFromPrincipal(principal, "test/foo", pm.ALLOW_ACTION, pm.EXPIRE_NEVER);
  pm.addFromPrincipal(principal, "test/foo", pm.DENY_ACTION, pm.EXPIRE_NEVER);

  check_enumerator(principal, [["test/foo", pm.DENY_ACTION, pm.EXPIRE_NEVER]]);

  // Let's reload the DB.
  Services.obs.notifyObservers(null, "testonly-reload-permissions-from-disk");
  Assert.ok(pm.all.length === 1);
  check_enumerator(principal, [["test/foo", pm.DENY_ACTION, pm.EXPIRE_NEVER]]);

  info("Cleanup");
  pm.removeAll();
  check_enumerator(principal, []);
});
