/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let pm;

// Create a principal based on the { origin, appId, browserElement }.
function createPrincipal(aOrigin, aAppId, aBrowserElement)
{
  return Services.scriptSecurityManager.getAppCodebasePrincipal(NetUtil.newURI(aOrigin), aAppId, aBrowserElement);
}

// Return the subject required by 'webapps-clear-data' notification.
function getSubject(aAppId, aBrowserOnly)
{
  return {
    appId: aAppId,
    browserOnly: aBrowserOnly,
    QueryInterface: XPCOMUtils.generateQI([Ci.mozIApplicationClearPrivateDataParams])
  };
}

// Use aEntries to create principals, add permissions to them and check that they have them.
// Then, it is notifying 'webapps-clear-data' with the given aSubject and check if the permissions
// of principals[i] matches the permission in aResults[i].
function test(aEntries, aSubject, aResults)
{
  let principals = [];

  for (entry of aEntries) {
    principals.push(createPrincipal(entry.origin, entry.appId, entry.browserElement));
  }

  for (principal of principals) {
    do_check_eq(pm.testPermissionFromPrincipal(principal, "test/webapps-clear"), pm.UNKNOWN_ACTION);
    pm.addFromPrincipal(principal, "test/webapps-clear", pm.ALLOW_ACTION, pm.EXPIRE_NEVER, 0);
    do_check_eq(pm.testPermissionFromPrincipal(principal, "test/webapps-clear"), pm.ALLOW_ACTION);
  }

  Services.obs.notifyObservers(aSubject, 'webapps-clear-data', null);

  var length = aEntries.length;
  for (let i=0; i<length; ++i) {
    do_check_eq(pm.testPermissionFromPrincipal(principals[i], 'test/webapps-clear'), aResults[i]);

    // Remove allowed actions.
    if (aResults[i] == pm.ALLOW_ACTION) {
      pm.removeFromPrincipal(principals[i], 'test/webapps-clear');
    }
  }
}

function run_test()
{
  do_get_profile();

  pm = Cc["@mozilla.org/permissionmanager;1"]
         .getService(Ci.nsIPermissionManager);

  let entries = [
    { origin: 'http://example.com', appId: 1, browserElement: false },
    { origin: 'http://example.com', appId: 1, browserElement: true },
    { origin: 'http://example.com', appId: Ci.nsIScriptSecurityManager.NO_APPID, browserElement: false },
    { origin: 'http://example.com', appId: 2, browserElement: false },
  ];

  // In that case, all permissions from app 1 should be removed but not the other ones.
  test(entries, getSubject(1, false), [ pm.UNKNOWN_ACTION, pm.UNKNOWN_ACTION, pm.ALLOW_ACTION, pm.ALLOW_ACTION ]);

  // In that case, only the permissions of app 1 related to a browserElement should be removed.
  // All the other permissions should stay.
  test(entries, getSubject(1, true), [ pm.ALLOW_ACTION, pm.UNKNOWN_ACTION, pm.ALLOW_ACTION, pm.ALLOW_ACTION ]);
}
