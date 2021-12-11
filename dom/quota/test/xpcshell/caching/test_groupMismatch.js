/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that the group loaded from the origin table
 * gets updated (if necessary) before quota initialization for the given origin.
 */

async function testSteps() {
  const principal = getPrincipal("https://foo.bar.mozilla-iot.org");
  const originUsage = 100;

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains one initialized origin directory with simple database
  // data, a script for origin initialization and the storage database:
  // - storage/default/https+++foo.bar.mozilla-iot.org
  // - create_db.js
  // - storage.sqlite
  // The file create_db.js in the package was run locally, specifically it was
  // temporarily added to xpcshell.ini and then executed:
  //   mach xpcshell-test --interactive dom/quota/test/xpcshell/create_db.js
  // Note: to make it become the profile in the test, additional manual steps
  // are needed.
  // 1. Manually change the group and accessed values in the origin table in
  //    storage.sqlite by running this SQL statement:
  //    UPDATE origin SET group_ = 'mozilla-iot.org', accessed = 0
  // 2. Manually change the group in .metadata-v2 from "bar.mozilla-iot.org" to
  //    "mozilla-iot.org".
  // 3. Remove the folder "storage/temporary".
  // 4. Remove the file "storage/ls-archive.sqlite".
  installPackage("groupMismatch_profile");

  request = getOriginUsage(principal, /* fromMemory */ true);
  await requestFinished(request);

  is(request.result.usage, originUsage, "Correct origin usage");
}
