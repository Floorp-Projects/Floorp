/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Verify that temporary storage initialization will notice a removed origin
 * that the cache has data for and which indicates the origin was accessed
 * during the last run. Currently, we expect LoadQuotaFromCache to fail because
 * of this inconsistency and to fall back to full initialization.
 */

async function testSteps() {
  const principal = getPrincipal("http://example.com");
  const originUsage = 0;

  info("Setting pref");

  // The packaged profile will have a different build ID and we would treat the
  // cache as invalid if we didn't bypass this check.
  Services.prefs.setBoolPref("dom.quotaManager.caching.checkBuildId", false);

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains empty default storage, a script for origin
  // initialization and the storage database:
  // - storage/default
  // - create_db.js
  // - storage.sqlite
  // The file create_db.js in the package was run locally, specifically it was
  // temporarily added to xpcshell.ini and then executed:
  //   mach xpcshell-test --interactive dom/quota/test/xpcshell/create_db.js
  // Note: to make it become the profile in the test, additional manual steps
  // are needed.
  // 1. Remove the folder "storage/default/http+++example.com".
  // 2. Remove the folder "storage/temporary".
  // 3. Remove the file "storage/ls-archive.sqlite".
  installPackage("removedOrigin_profile");

  info("Initializing");

  request = init();
  await requestFinished(request);

  info("Initializing temporary storage");

  request = initTemporaryStorage();
  await requestFinished(request);

  info("Getting origin usage");

  request = getOriginUsage(principal, /* fromMemory */ true);
  await requestFinished(request);

  is(request.result.usage, originUsage, "Correct origin usage");
}
