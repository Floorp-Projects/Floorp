/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const principal = getPrincipal("https://foo.example.com/");

  info("Setting pref");

  // The packaged profile will have a different build ID and we would treat the
  // cache as invalid if we didn't bypass this check.
  Services.prefs.setBoolPref("dom.quotaManager.caching.checkBuildId", false);

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains one initialized origin directory and the storage
  // database:
  // - storage/default/https+++foo.example.com
  // - storage.sqlite
  // The file make_unsetLastAccessTime.js was run locally, specifically it was
  // temporarily enabled in xpcshell.ini and then executed:
  // mach test --interactive dom/quota/test/xpcshell/caching/make_unsetLastAccessTime.js
  // Note: to make it become the profile in the test, additional manual steps
  // are needed.
  // 1. Remove the folder "storage/temporary".
  // 2. Remove the file "storage/ls-archive.sqlite".
  installPackage("unsetLastAccessTime_profile");

  info("Getting full origin metadata");

  request = getFullOriginMetadata("default", principal);
  await requestFinished(request);

  info("Verifying last access time");

  ok(
    BigInt(request.result.lastAccessTime) != INT64_MIN,
    "Correct last access time"
  );
}
