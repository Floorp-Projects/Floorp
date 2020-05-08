/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that persisted origins are always bounded by
 * the global limit.
 */

loadScript("dom/quota/test/common/file.js");

async function testSteps() {
  const globalLimitKB = 1;

  const principal = getPrincipal("https://persisted.example.com");

  info("Setting limits");

  setGlobalLimit(globalLimitKB);

  let request = clear();
  await requestFinished(request);

  for (let initializeStorageBeforePersist of [false, true]) {
    if (initializeStorageBeforePersist) {
      info("Initializing");

      request = init();
      await requestFinished(request);

      info("Initializing the temporary storage");

      request = initTemporaryStorage();
      await requestFinished(request);
    }

    info("Persisting an origin");

    request = persist(principal);
    await requestFinished(request);

    info("Verifying the persisted origin is bounded by global limit");

    let database = getSimpleDatabase(principal);

    info("Opening a database for the persisted origin");

    request = database.open("data");
    await requestFinished(request);

    try {
      info("Writing over the limit shouldn't succeed");

      request = database.write(getBuffer(globalLimitKB * 1024 + 1));
      await requestFinished(request);

      ok(false, "Should have thrown");
    } catch (ex) {
      ok(true, "Should have thrown");
      ok(ex === NS_ERROR_FILE_NO_DEVICE_SPACE, "Threw right code");
    }

    info("Closing the database and clearing");

    request = database.close();
    await requestFinished(request);

    request = clear();
    await requestFinished(request);
  }

  info("Resetting limits");

  resetGlobalLimit();

  request = reset();
  await requestFinished(request);
}
