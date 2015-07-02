/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// The origin we use in most of the tests.
const TEST_ORIGIN = NetUtil.newURI("http://example.org");
const TEST_ORIGIN_HTTPS = NetUtil.newURI("https://example.org");
const TEST_ORIGIN_2 = NetUtil.newURI("http://example.com");
const TEST_ORIGIN_3 = NetUtil.newURI("https://example2.com:8080");
const TEST_PERMISSION = "test-permission";
Components.utils.import("resource://gre/modules/Promise.jsm");

function promiseTimeout(delay) {
  let deferred = Promise.defer();
  do_timeout(delay, deferred.resolve);
  return deferred.promise;
}

function run_test() {
  run_next_test();
}

add_task(function* do_test() {
  // setup a profile.
  do_get_profile();

  // create a file in the temp directory with the defaults.
  let file = do_get_tempdir();
  file.append("test_default_permissions");

  // write our test data to it.
  let ostream = Cc["@mozilla.org/network/file-output-stream;1"].
                createInstance(Ci.nsIFileOutputStream);
  ostream.init(file, -1, 0666, 0);
  let conv = Cc["@mozilla.org/intl/converter-output-stream;1"].
             createInstance(Ci.nsIConverterOutputStream);
  conv.init(ostream, "UTF-8", 0, 0);

  conv.writeString("# this is a comment\n");
  conv.writeString("\n"); // a blank line!
  conv.writeString("host\t" + TEST_PERMISSION + "\t1\t" + TEST_ORIGIN.host + "\n");
  conv.writeString("host\t" + TEST_PERMISSION + "\t1\t" + TEST_ORIGIN_2.host + "\n");
  conv.writeString("origin\t" + TEST_PERMISSION + "\t1\t" + TEST_ORIGIN_3.spec + "\n");
  conv.writeString("origin\t" + TEST_PERMISSION + "\t1\t" + TEST_ORIGIN.spec + "^appId=1000&inBrowser=1\n");
  ostream.close();

  // Set the preference used by the permission manager so the file is read.
  Services.prefs.setCharPref("permissions.manager.defaultsUrl", "file://" + file.path);

  // initialize the permission manager service - it will read that default.
  let pm = Cc["@mozilla.org/permissionmanager;1"].
           getService(Ci.nsIPermissionManager);

  // test the default permission was applied.
  let principal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(TEST_ORIGIN);
  let principalHttps = Services.scriptSecurityManager.getNoAppCodebasePrincipal(TEST_ORIGIN_HTTPS);
  let principal2 = Services.scriptSecurityManager.getNoAppCodebasePrincipal(TEST_ORIGIN_2);
  let principal3 = Services.scriptSecurityManager.getNoAppCodebasePrincipal(TEST_ORIGIN_3);
  let principal4 = Services.scriptSecurityManager.getAppCodebasePrincipal(TEST_ORIGIN, 1000, true);
  let principal5 = Services.scriptSecurityManager.getAppCodebasePrincipal(TEST_ORIGIN_3, 1000, true);

  do_check_eq(Ci.nsIPermissionManager.ALLOW_ACTION,
              pm.testPermissionFromPrincipal(principal, TEST_PERMISSION));
  do_check_eq(Ci.nsIPermissionManager.ALLOW_ACTION,
              pm.testPermissionFromPrincipal(principalHttps, TEST_PERMISSION));
  do_check_eq(Ci.nsIPermissionManager.ALLOW_ACTION,
              pm.testPermissionFromPrincipal(principal3, TEST_PERMISSION));
  do_check_eq(Ci.nsIPermissionManager.ALLOW_ACTION,
              pm.testPermissionFromPrincipal(principal4, TEST_PERMISSION));

  // Didn't add
  do_check_eq(Ci.nsIPermissionManager.UNKNOWN_ACTION,
              pm.testPermissionFromPrincipal(principal5, TEST_PERMISSION));

  // the permission should exist in the enumerator.
  do_check_eq(Ci.nsIPermissionManager.ALLOW_ACTION, findCapabilityViaEnum(TEST_ORIGIN));
  do_check_eq(Ci.nsIPermissionManager.ALLOW_ACTION, findCapabilityViaEnum(TEST_ORIGIN_3));

  // but should not have been written to the DB
  yield checkCapabilityViaDB(null);

  // remove all should not throw and the default should remain
  pm.removeAll();

  do_check_eq(Ci.nsIPermissionManager.ALLOW_ACTION,
              pm.testPermissionFromPrincipal(principal, TEST_PERMISSION));
  do_check_eq(Ci.nsIPermissionManager.ALLOW_ACTION,
              pm.testPermissionFromPrincipal(principal3, TEST_PERMISSION));
  do_check_eq(Ci.nsIPermissionManager.ALLOW_ACTION,
              pm.testPermissionFromPrincipal(principal4, TEST_PERMISSION));

  // Asking for this permission to be removed should result in that permission
  // having UNKNOWN_ACTION
  pm.removeFromPrincipal(principal, TEST_PERMISSION);
  do_check_eq(Ci.nsIPermissionManager.UNKNOWN_ACTION,
              pm.testPermissionFromPrincipal(principal, TEST_PERMISSION));
  // and we should have this UNKNOWN_ACTION reflected in the DB
  yield checkCapabilityViaDB(Ci.nsIPermissionManager.UNKNOWN_ACTION);
  // but the permission should *not* appear in the enumerator.
  do_check_eq(null, findCapabilityViaEnum());

  // and a subsequent RemoveAll should restore the default
  pm.removeAll();

  do_check_eq(Ci.nsIPermissionManager.ALLOW_ACTION,
              pm.testPermissionFromPrincipal(principal, TEST_PERMISSION));
  // and allow it to again be seen in the enumerator.
  do_check_eq(Ci.nsIPermissionManager.ALLOW_ACTION, findCapabilityViaEnum());

  // now explicitly add a permission - this too should override the default.
  pm.addFromPrincipal(principal, TEST_PERMISSION, Ci.nsIPermissionManager.DENY_ACTION);

  // it should be reflected in a permission check, in the enumerator and the DB
  do_check_eq(Ci.nsIPermissionManager.DENY_ACTION,
              pm.testPermissionFromPrincipal(principal, TEST_PERMISSION));
  do_check_eq(Ci.nsIPermissionManager.DENY_ACTION, findCapabilityViaEnum());
  yield checkCapabilityViaDB(Ci.nsIPermissionManager.DENY_ACTION);

  // explicitly add a different permission - in this case we are no longer
  // replacing the default, but instead replacing the replacement!
  pm.addFromPrincipal(principal, TEST_PERMISSION, Ci.nsIPermissionManager.PROMPT_ACTION);

  // it should be reflected in a permission check, in the enumerator and the DB
  do_check_eq(Ci.nsIPermissionManager.PROMPT_ACTION,
              pm.testPermissionFromPrincipal(principal, TEST_PERMISSION));
  do_check_eq(Ci.nsIPermissionManager.PROMPT_ACTION, findCapabilityViaEnum());
  yield checkCapabilityViaDB(Ci.nsIPermissionManager.PROMPT_ACTION);

  // --------------------------------------------------------------
  // check default permissions and removeAllSince work as expected.
  pm.removeAll(); // ensure only defaults are there.

  // default for both principals is allow.
  do_check_eq(Ci.nsIPermissionManager.ALLOW_ACTION,
              pm.testPermissionFromPrincipal(principal, TEST_PERMISSION));
  do_check_eq(Ci.nsIPermissionManager.ALLOW_ACTION,
              pm.testPermissionFromPrincipal(principal2, TEST_PERMISSION));

  // Add a default override for TEST_ORIGIN_2 - this one should *not* be
  // restored in removeAllSince()
  pm.addFromPrincipal(principal2, TEST_PERMISSION, Ci.nsIPermissionManager.DENY_ACTION);
  do_check_eq(Ci.nsIPermissionManager.DENY_ACTION,
              pm.testPermissionFromPrincipal(principal2, TEST_PERMISSION));
  yield promiseTimeout(20);

  let since = Number(Date.now());
  yield promiseTimeout(20);

  // explicitly add a permission which overrides the default for the first
  // principal - this one *should* be removed by removeAllSince.
  pm.addFromPrincipal(principal, TEST_PERMISSION, Ci.nsIPermissionManager.DENY_ACTION);
  do_check_eq(Ci.nsIPermissionManager.DENY_ACTION,
              pm.testPermissionFromPrincipal(principal, TEST_PERMISSION));

  // do a removeAllSince.
  pm.removeAllSince(since);

  // the default for the first principal should re-appear as we modified it
  // later then |since|
  do_check_eq(Ci.nsIPermissionManager.ALLOW_ACTION,
              pm.testPermissionFromPrincipal(principal, TEST_PERMISSION));

  // but the permission for principal2 should remain as we added that before |since|.
  do_check_eq(Ci.nsIPermissionManager.DENY_ACTION,
              pm.testPermissionFromPrincipal(principal2, TEST_PERMISSION));

  // remove the temp file we created.
  file.remove(false);
});

// use an enumerator to find the requested permission.  Returns the permission
// value (ie, the "capability" in nsIPermission parlance) or null if it can't
// be found.
function findCapabilityViaEnum(origin = TEST_ORIGIN, type = TEST_PERMISSION) {
  let result = undefined;
  let e = Services.perms.enumerator;
  while (e.hasMoreElements()) {
    let perm = e.getNext().QueryInterface(Ci.nsIPermission);
    if (perm.matchesURI(origin, true) &&
        perm.type == type) {
      if (result !== undefined) {
        // we've already found one previously - that's bad!
        do_throw("enumerator found multiple entries");
      }
      result = perm.capability;
    }
  }
  return result || null;
}

// A function to check the DB has the specified capability.  As the permission
// manager uses async DB operations without a completion callback, the
// distinct possibility exists that our checking of the DB will happen before
// the permission manager update has completed - so we just retry a few times.
// Returns a promise.
function checkCapabilityViaDB(expected, origin = TEST_ORIGIN, type = TEST_PERMISSION) {
  let deferred = Promise.defer();
  let count = 0;
  let max = 20;
  let do_check = () => {
    let got = findCapabilityViaDB(origin, type);
    if (got == expected) {
      // the do_check_eq() below will succeed - which is what we want.
      do_check_eq(got, expected, "The database has the expected value");
      deferred.resolve();
      return;
    }
    // value isn't correct - see if we've retried enough
    if (count++ == max) {
      // the do_check_eq() below will fail - which is what we want.
      do_check_eq(got, expected, "The database wasn't updated with the expected value");
      deferred.resolve();
      return;
    }
    // we can retry...
    do_timeout(100, do_check);
  }
  do_check();
  return deferred.promise;
}

// use the DB to find the requested permission.   Returns the permission
// value (ie, the "capability" in nsIPermission parlance) or null if it can't
// be found.
function findCapabilityViaDB(origin = TEST_ORIGIN, type = TEST_PERMISSION) {
  let principal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(origin);
  let originStr = principal.origin;

  let file = Services.dirsvc.get("ProfD", Ci.nsIFile);
  file.append("permissions.sqlite");

  let storage = Cc["@mozilla.org/storage/service;1"]
                  .getService(Ci.mozIStorageService);

  let connection = storage.openDatabase(file);

  let query = connection.createStatement(
      "SELECT permission FROM moz_hosts WHERE origin = :origin AND type = :type");
  query.bindByName("origin", originStr);
  query.bindByName("type", type);

  if (!query.executeStep()) {
    // no row
    return null;
  }
  let result = query.getInt32(0);
  if (query.executeStep()) {
    // this is bad - we never expect more than 1 row here.
    do_throw("More than 1 row found!")
  }
  return result;
}
