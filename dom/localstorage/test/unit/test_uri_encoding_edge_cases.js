/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test verifies that group and origin strings for URIs with special
 * characters are consistent between calling
 * EnsureQuotaForOringin/EnsureOriginIsInitailized and GetQuotaObject in
 * PrepareDatastoreOp, so writing to local storage won't cause a crash because
 * of a null quota object. See bug 1516333.
 */

async function testSteps() {
  /**
   * The edge cases are specified in this array of origins. Each edge case must
   * contain two properties uri and path (origin directory path relative to the
   * profile directory).
   */
  const origins = [
    {
      uri: "file:///test'.html",
      path: "storage/default/file++++test'.html",
    },
    {
      uri: "file:///test>.html",
      path: "storage/default/file++++test%3E.html",
    },
  ];

  info("Setting prefs");

  Services.prefs.setBoolPref("dom.storage.next_gen", true);

  for (let origin of origins) {
    const principal = getPrincipal(origin.uri);

    let originDir = getRelativeFile(origin.path);

    info("Checking the origin directory existence");

    ok(
      !originDir.exists(),
      `The origin directory ${origin.path} should not exists`
    );

    info("Getting storage");

    let storage = getLocalStorage(principal);

    info("Adding item");

    storage.setItem("foo", "bar");

    info("Resetting origin");

    // This forces any pending changes to be flushed to disk (including origin
    // directory creation).
    let request = resetOrigin(principal);
    await requestFinished(request);

    info("Checking the origin directory existence");

    ok(originDir.exists(), `The origin directory ${origin.path} should exist`);
  }
}
