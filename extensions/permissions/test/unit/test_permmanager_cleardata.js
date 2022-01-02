/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var pm;

// Create a principal based on the { origin, originAttributes }.
function createPrincipal(aOrigin, aOriginAttributes) {
  return Services.scriptSecurityManager.createContentPrincipal(
    NetUtil.newURI(aOrigin),
    aOriginAttributes
  );
}

function getData(aPattern) {
  return JSON.stringify(aPattern);
}

// Use aEntries to create principals, add permissions to them and check that they have them.
// Then, it is removing origin attributes with the given aData and check if the permissions
// of principals[i] matches the permission in aResults[i].
function test(aEntries, aData, aResults) {
  let principals = [];

  for (const entry of aEntries) {
    principals.push(createPrincipal(entry.origin, entry.originAttributes));
  }

  for (const principal of principals) {
    Assert.equal(
      pm.testPermissionFromPrincipal(principal, "test/clear-origin"),
      pm.UNKNOWN_ACTION
    );
    pm.addFromPrincipal(
      principal,
      "test/clear-origin",
      pm.ALLOW_ACTION,
      pm.EXPIRE_NEVER,
      0
    );
    Assert.equal(
      pm.testPermissionFromPrincipal(principal, "test/clear-origin"),
      pm.ALLOW_ACTION
    );
  }

  // `clear-origin-attributes-data` notification is removed from permission
  // manager
  pm.removePermissionsWithAttributes(aData);

  var length = aEntries.length;
  for (let i = 0; i < length; ++i) {
    Assert.equal(
      pm.testPermissionFromPrincipal(principals[i], "test/clear-origin"),
      aResults[i]
    );

    // Remove allowed actions.
    if (aResults[i] == pm.ALLOW_ACTION) {
      pm.removeFromPrincipal(principals[i], "test/clear-origin");
    }
  }
}

function run_test() {
  do_get_profile();

  pm = Services.perms;

  let entries = [
    { origin: "http://example.com", originAttributes: {} },
    {
      origin: "http://example.com",
      originAttributes: { inIsolatedMozBrowser: true },
    },
  ];

  // In that case, all permissions should be removed.
  test(entries, getData({}), [
    pm.UNKNOWN_ACTION,
    pm.UNKNOWN_ACTION,
    pm.ALLOW_ACTION,
    pm.ALLOW_ACTION,
  ]);

  // In that case, only the permissions related to a browserElement should be removed.
  // All the other permissions should stay.
  test(entries, getData({ inIsolatedMozBrowser: true }), [
    pm.ALLOW_ACTION,
    pm.UNKNOWN_ACTION,
    pm.ALLOW_ACTION,
    pm.ALLOW_ACTION,
  ]);
}
