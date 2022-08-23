/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This test is mainly to verify that we are able to recover from a situation
// in which the padding file and the padding column are missing during origin
// initialization. This was originally reported in bug 1425146 comment 39.
// The situation can be described as follows:
// 1. A profile is used in FF57. The storage version is 2.1. There's no cache
//    storage for http://www.mozilla.org
// 2. The same profile is used in FF56. The storage version is still 2.1 which
//    doesn't prevent storage system from working since minor upgrades are
//    backwards-compatible. The cache storage for http://www.mozilla.org is
//    created with schema version 25 (without any padding stuff).
// 3. The profile is used in FF57 again. Zero padding files for existing cache
//    storages are not created because storage is already at version 2.1.
//    Storage is being initialized and a missing padding file triggers padding
//    size computation from the cache database with schema version 25. Since
//    the computation happens before any real DOM cache operation, the database
//    is not upgraded to schema version 26, so the padding column is missing.

add_task(async function testSteps() {
  // The profile contains one cache storage, a script for cache creation and
  // the storage database:
  // - storage/default/http+++www.mozilla.org/cache
  // - create_cache.js
  // - storage.sqlite
  // The file create_cache.js in the package was run locally, specifically it
  // was temporarily added to xpcshell.ini and then executed:
  //   mach xpcshell-test --interactive dom/cache/test/xpcshell/create_cache.js
  // Note: it must be executed in FF56 and it only creates the directory
  // "storage/default/chrome/cache" and the file "storage.sqlite". To make it
  // become the profile in the test, additional manual steps are needed.
  // 1. Create "http+++www.mozilla.org" folder under the ""storage/default".
  // 2. Copy the "cache" folder under the "storage/default/chrome" to
  //    "storage/default/http+++www.mozilla.org".
  // 3. Remove the folder "storage/default/chrome"
  // 4. Remove the folder "storage/temporary".
  // 5. Add "create_cache.js".
  // 6. Replace the "storage.sqlite" created by FF56 (storage v2.0) with the
  //    "storage.sqlite" created by FF57 (storage v2.1)
  create_test_profile("bug1425146_profile.zip");

  try {
    await caches.open("test");
    ok(true, "Should not have thrown");
  } catch (ex) {
    ok(false, "Should not have thrown");
  }
});
