/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that normally the oldest origin will be
 * evicted if the global limit is reached, but if the oldest origin is
 * persisted, then it won't be evicted.
 */

loadScript("dom/quota/test/common/file.js");

async function fillOrigin(principal, size) {
  let database = getSimpleDatabase(principal);

  let request = database.open("data");
  await requestFinished(request);

  try {
    request = database.write(getBuffer(size));
    await requestFinished(request);
    ok(true, "Should not have thrown");
  } catch (ex) {
    ok(false, "Should not have thrown");
  }

  request = database.close();
  await requestFinished(request);
}

async function testSteps() {
  // The group limit is calculated as 20% of the global limit and the minimum
  // value of the group limit is 10 MB.

  const groupLimitKB = 10 * 1024;
  const globalLimitKB = groupLimitKB * 5;

  setGlobalLimit(globalLimitKB);

  let request = clear();
  await requestFinished(request);

  for (let persistOldestOrigin of [false, true]) {
    info(
      "Testing " +
        (persistOldestOrigin ? "with" : "without") +
        " persisting the oldest origin"
    );

    info(
      "Step 1: Filling six separate origins to reach the global limit " +
        "and trigger eviction"
    );

    for (let index = 1; index <= 6; index++) {
      let spec = "http://example" + index + ".com";
      if (index == 1 && persistOldestOrigin) {
        request = persist(getPrincipal(spec));
        await requestFinished(request);
      }
      await fillOrigin(getPrincipal(spec), groupLimitKB * 1024);
    }

    info("Step 2: Verifying origin directories");

    for (let index = 1; index <= 6; index++) {
      let path = "storage/default/http+++example" + index + ".com";
      let file = getRelativeFile(path);
      if (index == (persistOldestOrigin ? 2 : 1)) {
        ok(!file.exists(), "The origin directory " + path + " doesn't exist");
      } else {
        ok(file.exists(), "The origin directory " + path + " does exist");
      }
    }

    request = clear();
    await requestFinished(request);
  }

  resetGlobalLimit();

  request = reset();
  await requestFinished(request);
}
