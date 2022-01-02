/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const origins = [
    {
      path: "storage/default/file+++UNIVERSAL_FILE_URI_ORIGIN",
      url: "file:///Test/test.html",
      persistence: "default",
    },
  ];

  info("Setting pref");

  SpecialPowers.setBoolPref("security.fileuri.strict_origin_policy", false);

  info("Initializing");

  let request = init();
  await requestFinished(request);

  info("Creating origin directories");

  for (let origin of origins) {
    let originDir = getRelativeFile(origin.path);
    originDir.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));
  }

  info("Initializing temporary storage");

  request = initTemporaryStorage();
  await requestFinished(request);

  info("Initializing origin directories");

  for (let origin of origins) {
    let result;

    try {
      request = initTemporaryOrigin(
        origin.persistence,
        getPrincipal(origin.url)
      );
      result = await requestFinished(request);

      ok(true, "Should not have thrown");
    } catch (ex) {
      ok(false, "Should not have thrown");
    }

    ok(!result, "Origin directory wasn't created");
  }
}
