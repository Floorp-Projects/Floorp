/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function check_enumerator(prefix, permissions) {
  let pm = Cc["@mozilla.org/permissionmanager;1"]
           .getService(Ci.nsIPermissionManager);

  let array = pm.getAllWithTypePrefix(prefix);
  for (let [uri, type, capability] of permissions) {
    let perm = array.shift();
    Assert.ok(perm != null);
    Assert.ok(perm.principal.URI.equals(uri));
    Assert.equal(perm.type, type);
    Assert.equal(perm.capability, capability);
    Assert.equal(perm.expireType, pm.EXPIRE_NEVER);
  }
  Assert.equal(array.length, 0);
}

function run_test() {
  let pm = Cc["@mozilla.org/permissionmanager;1"]
           .getService(Ci.nsIPermissionManager);

  let uri = NetUtil.newURI("http://example.com");
  let sub = NetUtil.newURI("http://sub.example.com");

  check_enumerator("test/", [ ]);

  pm.add(uri, "test/getallwithtypeprefix", pm.ALLOW_ACTION);
  pm.add(sub, "other-test/getallwithtypeprefix", pm.PROMPT_ACTION);
  check_enumerator("test/", [
    [ uri, "test/getallwithtypeprefix", pm.ALLOW_ACTION ],
  ]);

  pm.add(sub, "test/getallwithtypeprefix", pm.PROMPT_ACTION);
  check_enumerator("test/", [
    [ sub, "test/getallwithtypeprefix", pm.PROMPT_ACTION ],
    [ uri, "test/getallwithtypeprefix", pm.ALLOW_ACTION ],
  ]);

  check_enumerator("test/getallwithtypeprefix", [
    [ sub, "test/getallwithtypeprefix", pm.PROMPT_ACTION ],
    [ uri, "test/getallwithtypeprefix", pm.ALLOW_ACTION ],
  ]);

  // check that UNKNOWN_ACTION permissions are ignored
  pm.add(uri, "test/getallwithtypeprefix2", pm.UNKNOWN_ACTION);
  check_enumerator("test/", [
    [ sub, "test/getallwithtypeprefix", pm.PROMPT_ACTION ],
    [ uri, "test/getallwithtypeprefix", pm.ALLOW_ACTION ],
  ]);

  // check that permission updates are reflected
  pm.add(uri, "test/getallwithtypeprefix", pm.PROMPT_ACTION);
  check_enumerator("test/", [
    [ sub, "test/getallwithtypeprefix", pm.PROMPT_ACTION ],
    [ uri, "test/getallwithtypeprefix", pm.PROMPT_ACTION ],
  ]);

  // check that permission removals are reflected
  pm.remove(uri, "test/getallwithtypeprefix");
  check_enumerator("test/", [
    [ sub, "test/getallwithtypeprefix", pm.PROMPT_ACTION ],
  ]);

  pm.removeAll();
  check_enumerator("test/", [ ]);
}

