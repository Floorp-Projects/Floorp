/**
 *  This test is mainly to verify cache won't leave emptry directory.
 */

function resetStorage() {
  return new Promise(function(resolve, reject) {
    var qms = Services.qms;
    var request = qms.reset();
    request.callback = resolve;
  });
}

async function setUpEnv() {
  Services.prefs.setBoolPref("dom.quotaManager.testing", true);

  // We need this for generating the basic profile path
  create_test_profile("schema_25_profile.zip");

  // Trigger storage upgrade
  await caches.open("test");
  const cacheDir = getCacheDir();
  let morgueDir = cacheDir.clone();
  morgueDir.append("morgue");

  // clean the cache directoy
  for (let dir of morgueDir.directoryEntries) {
    for (let file of dir.directoryEntries) {
      file.remove(false);
    }
  }

  await resetStorage();
}

// This function ensure the directory with file shouldn't have been deleted and
// ensure the directory without file should've been deleted.
function verifyResult() {
  const cacheDir = getCacheDir();
  let morgueDir = cacheDir.clone();
  morgueDir.append("morgue");

  let foundEmpty = false;
  for (let dir of morgueDir.directoryEntries) {
    let empty = true;
    // eslint-disable-next-line no-unused-vars
    for (let file of dir.directoryEntries) {
      empty = false;
    }

    foundEmpty = foundEmpty || empty;
  }
  return !foundEmpty;
}

async function testSteps() {
  const url = "https://www.mozilla.org";

  info("Setting up environment");

  await setUpEnv();

  info("Test 0 - InitOrigin shouldn't leave an empty directoy");

  let cache = await caches.open("test");
  let response = await cache.match(url);
  ok(!!response, "Upgrade from 25 to 26 do succeed");
  ok(verifyResult(), "InitOrigin should clean all empty directories");

  info("Test 1 - DeleteOrphanedBodyFiles shouldn't leave an empty directoy");

  await cache.put(url, response.clone());
  // eslint-disable-next-line no-unused-vars
  let r = await cache.match(url);
  await cache.delete(url);
  await resetStorage();

  cache = await caches.open("test");

  // Extra operation to ensure the deletion is completed
  await cache.match(url);

  ok(verifyResult(), "Empty directory should be removed");

  await caches.delete("test");
}
