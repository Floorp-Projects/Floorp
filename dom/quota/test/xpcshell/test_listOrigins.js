/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const origins = [
    "https://example.com",
    "https://localhost",
    "https://www.mozilla.org",
  ];

  function verifyResult(result, expectedOrigins) {
    ok(result instanceof Array, "Got an array object");
    ok(result.length == expectedOrigins.length, "Correct number of elements");

    info("Sorting elements");

    result.sort(function(a, b) {
      if (a < b) {
        return -1;
      }
      if (a > b) {
        return 1;
      }
      return 0;
    });

    info("Verifying elements");

    for (let i = 0; i < result.length; i++) {
      ok(result[i] == expectedOrigins[i], "Result matches expected origin");
    }
  }

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Listing origins");

  request = listOrigins();
  await requestFinished(request);

  info("Verifying result");

  verifyResult(request.result, []);

  info("Clearing");

  request = clear();
  await requestFinished(request);

  info("Initializing origins");

  for (const origin of origins) {
    request = initStorageAndOrigin(getPrincipal(origin), "default");
    await requestFinished(request);
  }

  info("Listing origins");

  request = listOrigins();
  await requestFinished(request);

  info("Verifying result");

  verifyResult(request.result, origins);
}
