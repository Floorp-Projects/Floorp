/**
 *  This test is mainly to verify cache actions work as usual even there exists
 *  an unexpected padding file.
 */

function getTempPaddingFilePath() {
  let cacheDir = getCacheDir();
  let temporaryPaddingFile = cacheDir.clone();
  temporaryPaddingFile.append(".padding-tmp");
  return temporaryPaddingFile;
}

function createTempPaddingFile() {
  let temporaryPaddingFile = getTempPaddingFilePath();
  temporaryPaddingFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0644", 8));

  ok(
    temporaryPaddingFile.exists(),
    "Temporary padding file does be created by test"
  );
}

add_task(async function testSteps() {
  create_test_profile("schema_25_profile.zip");
  let cache = await caches.open("test");

  // Step 1: Verify cache.match won't fail when there is a temporary padding
  // file
  createTempPaddingFile();

  let response = await cache.match("https://www.mozilla.org");
  ok(!!response, "Upgrade from 25 to 26 do succeed");

  // Note: Only cache write actions(e.g. cache.put/add/addAll/delete) will
  // remove unexpected temporary padding file when writting an opaque response
  // into the file-system. Cache read actions(e.g. cache.keys/match) won't.
  let temporaryPaddingFile = getTempPaddingFilePath();
  ok(
    temporaryPaddingFile.exists(),
    "Temporary padding file doesn't be removed by cache.match"
  );

  // Step 2: Verify cache.put won't fail when there is a temporary padding
  // file
  await cache.put("https://foo.com", response);
  ok(
    !temporaryPaddingFile.exists(),
    "Temporary padding file does be removed by cache.put"
  );

  // Step 3: Verify cache.keys won't fail when there is a temporary padding
  // file
  createTempPaddingFile();

  let cacheEntries = await cache.keys("https://foo.com");
  ok(cacheEntries.length === 1, "Cache.put does succeed");

  ok(
    temporaryPaddingFile.exists(),
    "Temporary padding file doesn't be removed by cache.keys"
  );

  // Step 4: Verify cache.delete won't fail when there is a temporary padding
  // file
  await cache.delete("https://foo.com");
  ok(
    !temporaryPaddingFile.exists(),
    "Temporary padding file does be removed by cache.delete"
  );

  await caches.delete("test");
});
