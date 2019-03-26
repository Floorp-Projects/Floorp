/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is an unit test for clearStorageForPrincipal. It verifies that if
 * the removing client is the last client in the targeting origin, then it is
 * expected to remove the origin directory as well.
 */

async function testSteps()
{
  const testingOrigins = [
    {
      origin: "http://example.com",
      path: "storage/default/http+++example.com/",
      only_idb: false
    },
    {
      origin: "http://www.mozilla.org",
      path: "storage/default/http+++www.mozilla.org/",
      only_idb: true
    }
  ];
  const removingClient = "idb";

  info("Installing package to create the environment");
  // The package is manually created and it contains:
  // - storage/default/http+++www.mozilla.org/idb/
  // - storage/default/http+++www.example.com/idb/
  // - storage/default/http+++www.example.com/cache/
  installPackage("clearStorageForPrincipal_profile");

  let request;
  let file;
  for (let i = 0; i < testingOrigins.length; ++i) {
    info("Clearing");
    request = clearClient(getPrincipal(testingOrigins[i].origin), null,
                          removingClient);
    await requestFinished(request);

    info("Verifying");
    file = getRelativeFile(testingOrigins[i].path + removingClient);
    ok(!file.exists(), "Client file doesn't exist");

    file = getRelativeFile(testingOrigins[i].path);
    if (testingOrigins[i].only_idb) {
      ok(!file.exists(), "Origin file doesn't exist");
    } else {
      ok(file.exists(), "Origin file does exist");
    }
  }
}
