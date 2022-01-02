/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify indexedDB::QuotaClient::UpgradeStorageFrom1_0To2_0
 * method.
 */

async function testSteps() {
  const packages = [
    // Storage used by FF 49-54 (storage version 1.0 with idb directory).
    "version1_0_idb_profile",
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

  info("Initializing");

  request = init();
  await requestFinished(request);

  info("Verifying storage");

  verifyStorage(packages, "afterInit");
}
