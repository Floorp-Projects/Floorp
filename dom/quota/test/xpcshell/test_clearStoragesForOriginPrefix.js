/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify clearing by origin prefix.
 */

async function testSteps() {
  const packages = [
    "clearStoragesForOriginPrefix_profile",
    "defaultStorageDirectory_shared",
  ];

  const testData = [
    {
      origin: "http://example.com",
      persistence: null,
      key: "afterClearByOriginPrefix",
    },
    {
      origin: "http://example.com",
      persistence: "default",
      key: "afterClearByOriginPrefix_default",
    },
    {
      origin: "http://example.com",
      persistence: "persistent",
      key: "afterClearByOriginPrefix_persistent",
    },
    {
      origin: "http://example.com",
      persistence: "temporary",
      key: "afterClearByOriginPrefix_temporary",
    },
  ];

  for (const item of testData) {
    info("Clearing");

    let request = clear();
    await requestFinished(request);

    info("Verifying storage");

    verifyStorage(packages, "beforeInstall");

    info("Installing package");

    installPackages(packages);

    info("Verifying storage");

    verifyStorage(packages, "afterInstall");

    // TODO: Remove this block once origin clearing is able to ignore unknown
    //       directories.
    getRelativeFile("storage/default/invalid+++example.com").remove(false);
    getRelativeFile("storage/permanent/invalid+++example.com").remove(false);
    getRelativeFile("storage/temporary/invalid+++example.com").remove(false);

    info("Clearing origin by prefix");

    request = clearOriginsByPrefix(getPrincipal(item.origin), item.persistence);
    await requestFinished(request);

    info("Verifying storage");

    verifyStorage(packages, item.key, "afterClearByOriginPrefix");
  }
}
