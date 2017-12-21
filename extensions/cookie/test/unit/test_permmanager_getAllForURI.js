/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function check_enumerator(uri, permissions) {
  let pm = Cc["@mozilla.org/permissionmanager;1"]
           .getService(Ci.nsIPermissionManager);

  let enumerator = pm.getAllForURI(uri);
  for ([type, capability] of permissions) {
    let perm = enumerator.getNext();
    Assert.ok(perm != null);
    Assert.ok(perm.principal.URI.equals(uri));
    Assert.equal(perm.type, type);
    Assert.equal(perm.capability, capability);
    Assert.equal(perm.expireType, pm.EXPIRE_NEVER);
  }
  Assert.ok(!enumerator.hasMoreElements());
}

function run_test() {
  let pm = Cc["@mozilla.org/permissionmanager;1"]
           .getService(Ci.nsIPermissionManager);

  let uri = NetUtil.newURI("http://example.com");
  let sub = NetUtil.newURI("http://sub.example.com");

  check_enumerator(uri, [ ]);

  pm.add(uri, "test/getallforuri", pm.ALLOW_ACTION);
  check_enumerator(uri, [
    [ "test/getallforuri", pm.ALLOW_ACTION ]
  ]);

  // check that uris are matched exactly
  check_enumerator(sub, [ ]);

  pm.add(sub, "test/getallforuri", pm.PROMPT_ACTION);
  pm.add(sub, "test/getallforuri2", pm.DENY_ACTION);

  check_enumerator(sub, [
    [ "test/getallforuri", pm.PROMPT_ACTION ],
    [ "test/getallforuri2", pm.DENY_ACTION ]
  ]);

  // check that the original uri list has not changed
  check_enumerator(uri, [
    [ "test/getallforuri", pm.ALLOW_ACTION ]
  ]);

  // check that UNKNOWN_ACTION permissions are ignored
  pm.add(uri, "test/getallforuri2", pm.UNKNOWN_ACTION);
  pm.add(uri, "test/getallforuri3", pm.DENY_ACTION);

  check_enumerator(uri, [
    [ "test/getallforuri", pm.ALLOW_ACTION ],
    [ "test/getallforuri3", pm.DENY_ACTION ]
  ]);

  // check that permission updates are reflected
  pm.add(uri, "test/getallforuri", pm.PROMPT_ACTION);

  check_enumerator(uri, [
    [ "test/getallforuri", pm.PROMPT_ACTION ],
    [ "test/getallforuri3", pm.DENY_ACTION ]
  ]);

  // check that permission removals are reflected
  pm.remove(uri, "test/getallforuri");

  check_enumerator(uri, [
    [ "test/getallforuri3", pm.DENY_ACTION ]
  ]);

  pm.removeAll();
  check_enumerator(uri, [ ]);
  check_enumerator(sub, [ ]);
}

