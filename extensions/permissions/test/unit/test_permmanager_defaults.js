/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// The origin we use in most of the tests.
const TEST_ORIGIN = NetUtil.newURI("http://example.org");
const TEST_ORIGIN_HTTPS = NetUtil.newURI("https://example.org");
const TEST_ORIGIN_2 = NetUtil.newURI("http://example.com");
const TEST_ORIGIN_3 = NetUtil.newURI("https://example2.com:8080");
const TEST_PERMISSION = "test-permission";

function promiseTimeout(delay) {
  return new Promise(resolve => {
    do_timeout(delay, resolve);
  });
}

add_task(async function do_test() {
  // setup a profile.
  do_get_profile();

  // create a file in the temp directory with the defaults.
  let file = do_get_tempdir();
  file.append("test_default_permissions");

  // write our test data to it.
  let ostream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  ostream.init(file, -1, 0o666, 0);
  let conv = Cc["@mozilla.org/intl/converter-output-stream;1"].createInstance(
    Ci.nsIConverterOutputStream
  );
  conv.init(ostream, "UTF-8");

  conv.writeString("# this is a comment\n");
  conv.writeString("\n"); // a blank line!
  conv.writeString(
    "host\t" + TEST_PERMISSION + "\t1\t" + TEST_ORIGIN.host + "\n"
  );
  conv.writeString(
    "host\t" + TEST_PERMISSION + "\t1\t" + TEST_ORIGIN_2.host + "\n"
  );
  conv.writeString(
    "origin\t" + TEST_PERMISSION + "\t1\t" + TEST_ORIGIN_3.spec + "\n"
  );
  conv.writeString(
    "origin\t" + TEST_PERMISSION + "\t1\t" + TEST_ORIGIN.spec + "^inBrowser=1\n"
  );
  ostream.close();

  // Set the preference used by the permission manager so the file is read.
  Services.prefs.setCharPref(
    "permissions.manager.defaultsUrl",
    "file://" + file.path
  );

  // This will force the permission-manager to reload the data.
  Services.obs.notifyObservers(null, "testonly-reload-permissions-from-disk");

  let permIsolateUserContext = Services.prefs.getBoolPref(
    "permissions.isolateBy.userContext"
  );
  let permIsolatePrivateBrowsing = Services.prefs.getBoolPref(
    "permissions.isolateBy.privateBrowsing"
  );

  let pm = Services.perms;

  // test the default permission was applied.
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    TEST_ORIGIN,
    {}
  );
  let principalHttps = Services.scriptSecurityManager.createContentPrincipal(
    TEST_ORIGIN_HTTPS,
    {}
  );
  let principal2 = Services.scriptSecurityManager.createContentPrincipal(
    TEST_ORIGIN_2,
    {}
  );
  let principal3 = Services.scriptSecurityManager.createContentPrincipal(
    TEST_ORIGIN_3,
    {}
  );

  let attrs = { inIsolatedMozBrowser: true };
  let principal4 = Services.scriptSecurityManager.createContentPrincipal(
    TEST_ORIGIN,
    attrs
  );
  let principal5 = Services.scriptSecurityManager.createContentPrincipal(
    TEST_ORIGIN_3,
    attrs
  );

  attrs = { userContextId: 1 };
  let principal1UserContext = Services.scriptSecurityManager.createContentPrincipal(
    TEST_ORIGIN,
    attrs
  );
  attrs = { privateBrowsingId: 1 };
  let principal1PrivateBrowsing = Services.scriptSecurityManager.createContentPrincipal(
    TEST_ORIGIN,
    attrs
  );
  attrs = { firstPartyDomain: "cnn.com" };
  let principal7 = Services.scriptSecurityManager.createContentPrincipal(
    TEST_ORIGIN,
    attrs
  );
  attrs = { userContextId: 1, firstPartyDomain: "cnn.com" };
  let principal8 = Services.scriptSecurityManager.createContentPrincipal(
    TEST_ORIGIN,
    attrs
  );

  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal, TEST_PERMISSION)
  );
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principalHttps, TEST_PERMISSION)
  );
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal3, TEST_PERMISSION)
  );
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal4, TEST_PERMISSION)
  );
  // Depending on the prefs there are two scenarios here:
  // 1. We isolate by private browsing: The permission mgr should
  //    add default permissions for these principals too.
  // 2. We don't isolate by private browsing: The permission
  //    check will strip the private browsing origin attribute.
  //    In this case the used internally for the lookup is always principal1.
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal1PrivateBrowsing, TEST_PERMISSION)
  );

  // Didn't add
  Assert.equal(
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(principal5, TEST_PERMISSION)
  );

  // the permission should exist in the enumerator.
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    findCapabilityViaEnum(TEST_ORIGIN)
  );
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    findCapabilityViaEnum(TEST_ORIGIN_3)
  );

  // but should not have been written to the DB
  await checkCapabilityViaDB(null);

  // remove all should not throw and the default should remain
  pm.removeAll();

  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal, TEST_PERMISSION)
  );
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal3, TEST_PERMISSION)
  );
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal4, TEST_PERMISSION)
  );
  // Default permission should have also been added for private browsing.
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal1PrivateBrowsing, TEST_PERMISSION)
  );
  // make sure principals with userContextId use the same / different permissions
  // depending on pref state
  Assert.equal(
    permIsolateUserContext
      ? Ci.nsIPermissionManager.UNKNOWN_ACTION
      : Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal1UserContext, TEST_PERMISSION)
  );
  // make sure principals with a firstPartyDomain use different permissions
  Assert.equal(
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(principal7, TEST_PERMISSION)
  );
  Assert.equal(
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(principal8, TEST_PERMISSION)
  );

  // Asking for this permission to be removed should result in that permission
  // having UNKNOWN_ACTION
  pm.removeFromPrincipal(principal, TEST_PERMISSION);
  Assert.equal(
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(principal, TEST_PERMISSION)
  );
  // make sure principals with userContextId use the correct permissions
  // (Should be unknown with and without OA stripping )
  Assert.equal(
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(principal1UserContext, TEST_PERMISSION)
  );
  // If we isolate by private browsing, the permission should still be present
  // for the private browsing principal.
  Assert.equal(
    permIsolatePrivateBrowsing
      ? Ci.nsIPermissionManager.ALLOW_ACTION
      : Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(principal1PrivateBrowsing, TEST_PERMISSION)
  );
  // and we should have this UNKNOWN_ACTION reflected in the DB
  await checkCapabilityViaDB(Ci.nsIPermissionManager.UNKNOWN_ACTION);
  // but the permission should *not* appear in the enumerator.
  Assert.equal(null, findCapabilityViaEnum());

  // and a subsequent RemoveAll should restore the default
  pm.removeAll();

  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal, TEST_PERMISSION)
  );
  // Make sure default imports work for private browsing after removeAll.
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal1PrivateBrowsing, TEST_PERMISSION)
  );
  // make sure principals with userContextId share permissions depending on pref state
  Assert.equal(
    permIsolateUserContext
      ? Ci.nsIPermissionManager.UNKNOWN_ACTION
      : Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal1UserContext, TEST_PERMISSION)
  );
  // make sure principals with firstPartyDomain use different permissions
  Assert.equal(
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(principal7, TEST_PERMISSION)
  );
  Assert.equal(
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(principal8, TEST_PERMISSION)
  );
  // and allow it to again be seen in the enumerator.
  Assert.equal(Ci.nsIPermissionManager.ALLOW_ACTION, findCapabilityViaEnum());

  // now explicitly add a permission - this too should override the default.
  pm.addFromPrincipal(
    principal,
    TEST_PERMISSION,
    Ci.nsIPermissionManager.DENY_ACTION
  );

  // it should be reflected in a permission check, in the enumerator and the DB
  Assert.equal(
    Ci.nsIPermissionManager.DENY_ACTION,
    pm.testPermissionFromPrincipal(principal, TEST_PERMISSION)
  );
  // make sure principals with userContextId use the same / different permissions
  // depending on pref state
  Assert.equal(
    permIsolateUserContext
      ? Ci.nsIPermissionManager.UNKNOWN_ACTION
      : Ci.nsIPermissionManager.DENY_ACTION,
    pm.testPermissionFromPrincipal(principal1UserContext, TEST_PERMISSION)
  );
  // If we isolate by private browsing, we should still have the default perm
  // for the private browsing principal.
  Assert.equal(
    permIsolatePrivateBrowsing
      ? Ci.nsIPermissionManager.ALLOW_ACTION
      : Ci.nsIPermissionManager.DENY_ACTION,
    pm.testPermissionFromPrincipal(principal1PrivateBrowsing, TEST_PERMISSION)
  );
  // make sure principals with firstPartyDomain use different permissions
  Assert.equal(
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(principal7, TEST_PERMISSION)
  );
  Assert.equal(
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(principal8, TEST_PERMISSION)
  );
  Assert.equal(Ci.nsIPermissionManager.DENY_ACTION, findCapabilityViaEnum());
  await checkCapabilityViaDB(Ci.nsIPermissionManager.DENY_ACTION);

  // explicitly add a different permission - in this case we are no longer
  // replacing the default, but instead replacing the replacement!
  pm.addFromPrincipal(
    principal,
    TEST_PERMISSION,
    Ci.nsIPermissionManager.PROMPT_ACTION
  );

  // it should be reflected in a permission check, in the enumerator and the DB
  Assert.equal(
    Ci.nsIPermissionManager.PROMPT_ACTION,
    pm.testPermissionFromPrincipal(principal, TEST_PERMISSION)
  );
  // make sure principals with userContextId use the same / different permissions
  // depending on pref state
  Assert.equal(
    permIsolateUserContext
      ? Ci.nsIPermissionManager.UNKNOWN_ACTION
      : Ci.nsIPermissionManager.PROMPT_ACTION,
    pm.testPermissionFromPrincipal(principal1UserContext, TEST_PERMISSION)
  );
  // If we isolate by private browsing, we should still have the default perm
  // for the private browsing principal.
  Assert.equal(
    permIsolatePrivateBrowsing
      ? Ci.nsIPermissionManager.ALLOW_ACTION
      : Ci.nsIPermissionManager.PROMPT_ACTION,
    pm.testPermissionFromPrincipal(principal1PrivateBrowsing, TEST_PERMISSION)
  );
  // make sure principals with firstPartyDomain use different permissions
  Assert.equal(
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(principal7, TEST_PERMISSION)
  );
  Assert.equal(
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    pm.testPermissionFromPrincipal(principal8, TEST_PERMISSION)
  );
  Assert.equal(Ci.nsIPermissionManager.PROMPT_ACTION, findCapabilityViaEnum());
  await checkCapabilityViaDB(Ci.nsIPermissionManager.PROMPT_ACTION);

  // --------------------------------------------------------------
  // check default permissions and removeAllSince work as expected.
  pm.removeAll(); // ensure only defaults are there.

  // default for both principals is allow.
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal, TEST_PERMISSION)
  );
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal2, TEST_PERMISSION)
  );

  // Add a default override for TEST_ORIGIN_2 - this one should *not* be
  // restored in removeAllSince()
  pm.addFromPrincipal(
    principal2,
    TEST_PERMISSION,
    Ci.nsIPermissionManager.DENY_ACTION
  );
  Assert.equal(
    Ci.nsIPermissionManager.DENY_ACTION,
    pm.testPermissionFromPrincipal(principal2, TEST_PERMISSION)
  );
  await promiseTimeout(20);

  let since = Number(Date.now());
  await promiseTimeout(20);

  // explicitly add a permission which overrides the default for the first
  // principal - this one *should* be removed by removeAllSince.
  pm.addFromPrincipal(
    principal,
    TEST_PERMISSION,
    Ci.nsIPermissionManager.DENY_ACTION
  );
  Assert.equal(
    Ci.nsIPermissionManager.DENY_ACTION,
    pm.testPermissionFromPrincipal(principal, TEST_PERMISSION)
  );

  // do a removeAllSince.
  pm.removeAllSince(since);

  // the default for the first principal should re-appear as we modified it
  // later then |since|
  Assert.equal(
    Ci.nsIPermissionManager.ALLOW_ACTION,
    pm.testPermissionFromPrincipal(principal, TEST_PERMISSION)
  );

  // but the permission for principal2 should remain as we added that before |since|.
  Assert.equal(
    Ci.nsIPermissionManager.DENY_ACTION,
    pm.testPermissionFromPrincipal(principal2, TEST_PERMISSION)
  );

  // remove the temp file we created.
  file.remove(false);
});

// use an enumerator to find the requested permission.  Returns the permission
// value (ie, the "capability" in nsIPermission parlance) or null if it can't
// be found.
function findCapabilityViaEnum(origin = TEST_ORIGIN, type = TEST_PERMISSION) {
  let result = undefined;
  for (let perm of Services.perms.all) {
    if (perm.matchesURI(origin, true) && perm.type == type) {
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
function checkCapabilityViaDB(
  expected,
  origin = TEST_ORIGIN,
  type = TEST_PERMISSION
) {
  return new Promise(resolve => {
    let count = 0;
    let max = 20;
    let do_check = () => {
      let got = findCapabilityViaDB(origin, type);
      if (got == expected) {
        // the do_check_eq() below will succeed - which is what we want.
        Assert.equal(got, expected, "The database has the expected value");
        resolve();
        return;
      }
      // value isn't correct - see if we've retried enough
      if (count++ == max) {
        // the do_check_eq() below will fail - which is what we want.
        Assert.equal(
          got,
          expected,
          "The database wasn't updated with the expected value"
        );
        resolve();
        return;
      }
      // we can retry...
      do_timeout(100, do_check);
    };
    do_check();
  });
}

// use the DB to find the requested permission.   Returns the permission
// value (ie, the "capability" in nsIPermission parlance) or null if it can't
// be found.
function findCapabilityViaDB(origin = TEST_ORIGIN, type = TEST_PERMISSION) {
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    origin,
    {}
  );
  let originStr = principal.origin;

  let file = Services.dirsvc.get("ProfD", Ci.nsIFile);
  file.append("permissions.sqlite");

  let connection = Services.storage.openDatabase(file);

  let query = connection.createStatement(
    "SELECT permission FROM moz_perms WHERE origin = :origin AND type = :type"
  );
  query.bindByName("origin", originStr);
  query.bindByName("type", type);

  if (!query.executeStep()) {
    // no row
    return null;
  }
  let result = query.getInt32(0);
  if (query.executeStep()) {
    // this is bad - we never expect more than 1 row here.
    do_throw("More than 1 row found!");
  }
  return result;
}
