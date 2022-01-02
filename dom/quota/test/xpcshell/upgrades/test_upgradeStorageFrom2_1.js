/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify UpgradeStorageFrom2_1To2_2 method (removal of
 * obsolete origins, deprecated clients and unknown temporary files).
 */

async function testSteps() {
  const filePaths = [
    // Obsolete origins:
    "storage/default/chrome+++content+browser.xul",

    "storage/default/moz-safe-about+++home",

    // TODO: These three origins don't belong here! They were added one release
    //       later and the origin parser was fixed to handle these origins one
    //       release later as well, so users which already upgraded to 2.2 may
    //       still have issues related to these origins!
    "storage/default/about+home+1",

    "storage/default/about+home+1+q",

    // about:reader?url=xxx (before bug 1422456)
    "storage/default/about+reader+url=https%3A%2F%2Fexample.com",

    // Deprecated client:
    "storage/default/https+++example.com/asmjs",

    // Unknown temporary file:
    "storage/default/https+++example.com/idb/UUID123.tmp",
  ];

  const packages = [
    // Storage used by FF 57-67 (storage version 2.1 with obsolete origins, a
    // deprecated client and an unknown temporary file).
    "version2_1_profile",
    "../defaultStorageDirectory_shared",
  ];

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Verifying storage");

  verifyStorage(packages, "beforeInstall");

  info("Installing packages");

  installPackages(packages);

  info("Verifying storage");

  verifyStorage(packages, "afterInstall");

  info("Checking files and directories before upgrade (storage version 2.1)");

  for (const filePath of filePaths) {
    let file = getRelativeFile(filePath);
    let exists = file.exists();
    ok(exists, "File or directory does exist");
  }

  info("Initializing");

  // Initialize to trigger storage upgrade from version 2.1
  request = init();
  await requestFinished(request);

  info("Verifying storage");

  verifyStorage(packages, "afterInit");

  info("Checking files and directories after upgrade");

  for (const filePath of filePaths) {
    let file = getRelativeFile(filePath);
    let exists = file.exists();
    ok(!exists, "File or directory does not exist");
  }
}
