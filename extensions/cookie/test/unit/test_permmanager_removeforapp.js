/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  // initialize the permission manager service
  let ssm = Services.scriptSecurityManager;
  let pm = Services.perms;

  function mkPrin(uri, appId, inIsolatedMozBrowser) {
    return ssm.createCodebasePrincipal(Services.io.newURI(uri),
                                       {appId: appId, inIsolatedMozBrowser: inIsolatedMozBrowser});
  }

  function checkPerms(perms) {
    perms.forEach((perm) => {
      // Look up the expected permission
      Assert.equal(pm.getPermissionObject(mkPrin(perm[0],  perm[1], perm[2]),
                                          perm[3], true).capability,
                   perm[4], "Permission is expected in the permission database");
    });

    // Count the entries
    let count = 0;
    let enumerator = Services.perms.enumerator;
    while (enumerator.hasMoreElements()) { enumerator.getNext(); count++; }

    Assert.equal(count, perms.length, "There should be the right number of permissions in the DB");
  }

  checkPerms([]);

  let permissions = [
    ['http://google.com',  1001, false, 'a', 1],
    ['http://google.com',  1001, false, 'b', 1],
    ['http://mozilla.com', 1001, false, 'b', 1],
    ['http://mozilla.com', 1001, false, 'a', 1],

    ['http://google.com',  1001, true, 'a', 1],
    ['http://google.com',  1001, true, 'b', 1],
    ['http://mozilla.com', 1001, true, 'b', 1],
    ['http://mozilla.com', 1001, true, 'a', 1],

    ['http://google.com',  1011, false, 'a', 1],
    ['http://google.com',  1011, false, 'b', 1],
    ['http://mozilla.com', 1011, false, 'b', 1],
    ['http://mozilla.com', 1011, false, 'a', 1],
  ];

  permissions.forEach((perm) => {
    pm.addFromPrincipal(mkPrin(perm[0], perm[1], perm[2]), perm[3], perm[4]);
  });

  checkPerms(permissions);

  let remove_false_perms = [
    ['http://google.com',  1011, false, 'a', 1],
    ['http://google.com',  1011, false, 'b', 1],
    ['http://mozilla.com', 1011, false, 'b', 1],
    ['http://mozilla.com', 1011, false, 'a', 1],
  ];

  let attrs = { appId: 1001 };
  pm.removePermissionsWithAttributes(JSON.stringify(attrs));
  checkPerms(remove_false_perms);

  let restore = [
    ['http://google.com',  1001, false, 'a', 1],
    ['http://google.com',  1001, false, 'b', 1],
    ['http://mozilla.com', 1001, false, 'b', 1],
    ['http://mozilla.com', 1001, false, 'a', 1],

    ['http://google.com',  1001, true, 'a', 1],
    ['http://google.com',  1001, true, 'b', 1],
    ['http://mozilla.com', 1001, true, 'b', 1],
    ['http://mozilla.com', 1001, true, 'a', 1],
  ];

  restore.forEach((perm) => {
    pm.addFromPrincipal(mkPrin(perm[0],  perm[1], perm[2]), perm[3], perm[4]);
  });
  checkPerms(permissions);

  let remove_true_perms = [
    ['http://google.com',  1001, false, 'a', 1],
    ['http://google.com',  1001, false, 'b', 1],
    ['http://mozilla.com', 1001, false, 'b', 1],
    ['http://mozilla.com', 1001, false, 'a', 1],

    ['http://google.com',  1011, false, 'a', 1],
    ['http://google.com',  1011, false, 'b', 1],
    ['http://mozilla.com', 1011, false, 'b', 1],
    ['http://mozilla.com', 1011, false, 'a', 1],
  ];

  attrs = { appId: 1001,
            inIsolatedMozBrowser: true };
  pm.removePermissionsWithAttributes(JSON.stringify(attrs));
  checkPerms(remove_true_perms);
}
