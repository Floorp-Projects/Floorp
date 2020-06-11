/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that persisted origins are not constrained by
 * the group limit.  It consits of these steps:
 * - Set the limits as small as our limits allow.  This does result in needing
 *   to perform 10 megs of writes which is a lot for a test but not horrible.
 * - Create databases for 2 origins under the same group.
 * - Have the foo2 origin use up the shared group quota.
 * - Verify neither origin can write additional data (via a single byte write).
 * - Do navigator.storage.persist() for that foo2 origin.
 * - Verify that both origins can now write an additional byte.  This
 *   demonstrates that:
 *   - foo2 no longer counts against the group limit at all since foo1 can
 *     write a byte.
 *   - foo2 is no longer constrained by the group limit itself.
 */
async function testSteps() {
  // The group limit is calculated as 20% of the global limit and the minimum
  // value of the group limit is 10 MB.

  const groupLimitKB = 10 * 1024;
  const globalLimitKB = groupLimitKB * 5;

  const urls = ["http://foo1.example.com", "http://foo2.example.com"];

  const foo2Index = 1;

  let index;

  info("Setting limits");

  setGlobalLimit(globalLimitKB);

  let request = clear();
  await requestFinished(request);

  info("Opening databases");

  let databases = [];
  for (index = 0; index < urls.length; index++) {
    let database = getSimpleDatabase(getPrincipal(urls[index]));

    request = database.open("data");
    await requestFinished(request);

    databases.push(database);
  }

  info("Filling up the whole group");

  try {
    request = databases[foo2Index].write(new ArrayBuffer(groupLimitKB * 1024));
    await requestFinished(request);
    ok(true, "Should not have thrown");
  } catch (ex) {
    ok(false, "Should not have thrown");
  }

  info("Verifying no more data can be written");

  for (index = 0; index < urls.length; index++) {
    try {
      request = databases[index].write(new ArrayBuffer(1));
      await requestFinished(request);
      ok(false, "Should have thrown");
    } catch (ex) {
      ok(true, "Should have thrown");
      ok(ex == NS_ERROR_FILE_NO_DEVICE_SPACE, "Threw right code");
    }
  }

  info("Persisting origin");

  request = persist(getPrincipal(urls[foo2Index]));
  await requestFinished(request);

  info("Verifying more data data can be written");

  for (index = 0; index < urls.length; index++) {
    try {
      request = databases[index].write(new ArrayBuffer(1));
      await requestFinished(request);
      ok(true, "Should not have thrown");
    } catch (ex) {
      ok(false, "Should not have thrown");
    }
  }

  info("Closing databases");

  for (index = 0; index < urls.length; index++) {
    request = databases[index].close();
    await requestFinished(request);
  }

  finishTest();
}
