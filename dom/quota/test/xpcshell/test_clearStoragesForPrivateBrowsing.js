/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify clearing of storages for private browsing.
 */

async function testSteps() {
  const packages = [
    "clearStoragesForPrivateBrowsing_profile",
    "defaultStorageDirectory_shared",
  ];

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Verifying storage");

  verifyStorage(packages, "beforeInstall");

  info("Installing package");

  installPackages(packages);

  info("Verifying storage");

  verifyStorage(packages, "afterInstall");

  info("Clearing private browsing");

  request = clearPrivateBrowsing();
  await requestFinished(request);

  info("Verifying storage");

  verifyStorage(packages, "afterClearPrivateBrowsing");
}
