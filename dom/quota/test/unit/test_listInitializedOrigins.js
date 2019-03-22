/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function CreateTestEnvironment(origins)
{
  let request;
  for (let origin of origins) {
    request = initOrigin(getPrincipal(origin.origin), "default");
    await requestFinished(request);
  }

  request = reset();
  await requestFinished(request);
}

async function testSteps()
{
  const origins = [
    {
      origin: "https://example.com",
      path: "https+++example.com"
    },

    {
      origin: "https://localhost",
      path: "https+++localhost"
    },

    {
      origin: "https://www.mozilla.org",
      path: "https+++www.mozilla.org"
    }
  ];

  function verifyResult(result, origins) {
    ok(result instanceof Array, "Got an array object");
    ok(result.length == origins.length, "Correct number of elements");

    info("Sorting elements");

    result.sort(function(a, b) {
      let originA = a.origin
      let originB = b.origin

      if (originA < originB) {
        return -1;
      }
      if (originA > originB) {
        return 1;
      }
      return 0;
    });

    info("Verifying elements");

    for (let i = 0; i < result.length; i++) {
      let a = result[i];
      let b = origins[i];
      ok(a.origin == b.origin, "Origin equals");
    }
  }

  info("Creating test environment");

  await CreateTestEnvironment(origins);

  info("Getting origins before initializing the storage");

  await new Promise(resolve => {
    listInitializedOrigins(request => {
      verifyResult(request.result, []);
      resolve();
  })});

  info("Verifying result");

  let request = initTemporaryStorage();
  request = await requestFinished(request);

  info("Getting origins after initializing the storage");

  await new Promise(resolve => {
    listInitializedOrigins(request => {
      verifyResult(request.result, origins);
      resolve();
  })});
}
