/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  // initialize the permission manager service
  const kTestAddr = "test@example.org";
  const kType = "test-mailto";
  const kCapability = 1;

  // make a mailto: URI with parameters
  let uri = Services.io.newURI("mailto:" + kTestAddr + "?subject=test", null,
                               null);
  let origin = "mailto:" + kTestAddr;

  // add a permission entry for that URI
  Services.perms.add(uri, kType, kCapability);
  do_check_true(permission_exists(origin, kType, kCapability));

  // remove the permission, and make sure it was removed
  Services.perms.remove(uri, kType);
  do_check_false(permission_exists(origin, kType, kCapability));

  uri = Services.io.newURI("mailto:" + kTestAddr, null, null);
  Services.perms.add(uri, kType, kCapability);
  do_check_true(permission_exists(origin, kType, kCapability));

  Services.perms.remove(uri, kType);
  do_check_false(permission_exists(origin, kType, kCapability));
}

function permission_exists(aOrigin, aType, aCapability) {
  let e = Services.perms.enumerator;
  while (e.hasMoreElements()) {
    let perm = e.getNext().QueryInterface(Ci.nsIPermission);
    if (perm.principal.origin == aOrigin &&
        perm.type == aType &&
        perm.capability == aCapability) {
      return true;
    }
  }
  return false;
}
