/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var pm;

// Create a principal based on the { origin, originAttributes }.
function createPrincipal(aOrigin, aOriginAttributes)
{
  return Services.scriptSecurityManager.createCodebasePrincipal(NetUtil.newURI(aOrigin), aOriginAttributes);
}

// Return the data required by 'clear-origin-attributes-data' notification.
function getData(aPattern)
{
  return JSON.stringify(aPattern);
}

// Use aEntries to create principals, add permissions to them and check that they have them.
// Then, it is notifying 'clear-origin-attributes-data' with the given aData and check if the permissions
// of principals[i] matches the permission in aResults[i].
function test(aEntries, aData, aResults)
{
  let principals = [];

  for (entry of aEntries) {
    principals.push(createPrincipal(entry.origin, entry.originAttributes));
  }

  for (principal of principals) {
    Assert.equal(pm.testPermissionFromPrincipal(principal, "test/clear-origin"), pm.UNKNOWN_ACTION);
    pm.addFromPrincipal(principal, "test/clear-origin", pm.ALLOW_ACTION, pm.EXPIRE_NEVER, 0);
    Assert.equal(pm.testPermissionFromPrincipal(principal, "test/clear-origin"), pm.ALLOW_ACTION);
  }

  Services.obs.notifyObservers(null, 'clear-origin-attributes-data', aData);

  var length = aEntries.length;
  for (let i=0; i<length; ++i) {
    Assert.equal(pm.testPermissionFromPrincipal(principals[i], 'test/clear-origin'), aResults[i]);

    // Remove allowed actions.
    if (aResults[i] == pm.ALLOW_ACTION) {
      pm.removeFromPrincipal(principals[i], 'test/clear-origin');
    }
  }
}

function run_test()
{
  do_get_profile();

  pm = Cc["@mozilla.org/permissionmanager;1"]
         .getService(Ci.nsIPermissionManager);

  let entries = [
    { origin: 'http://example.com', originAttributes: { appId: 1 } },
    { origin: 'http://example.com', originAttributes: { appId: 1, inIsolatedMozBrowser: true } },
    { origin: 'http://example.com', originAttributes: {} },
    { origin: 'http://example.com', originAttributes: { appId: 2 } },
  ];

  // In that case, all permissions from app 1 should be removed but not the other ones.
  test(entries, getData({appId: 1}), [ pm.UNKNOWN_ACTION, pm.UNKNOWN_ACTION, pm.ALLOW_ACTION, pm.ALLOW_ACTION ]);

  // In that case, only the permissions of app 1 related to a browserElement should be removed.
  // All the other permissions should stay.
  test(entries, getData({appId: 1, inIsolatedMozBrowser: true}), [ pm.ALLOW_ACTION, pm.UNKNOWN_ACTION, pm.ALLOW_ACTION, pm.ALLOW_ACTION ]);
}
