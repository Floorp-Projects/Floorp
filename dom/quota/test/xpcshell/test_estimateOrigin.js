/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

loadScript("dom/quota/test/xpcshell/common/utils.js");

async function verifyOriginEstimation(principal, expectedUsage, expectedLimit) {
  info("Estimating origin");

  const request = estimateOrigin(principal);
  await requestFinished(request);

  is(request.result.usage, expectedUsage, "Correct usage");
  is(request.result.limit, expectedLimit, "Correct limit");
}

async function testSteps() {
  // The group limit is calculated as 20% of the global limit and the minimum
  // value of the group limit is 10 MB.

  const groupLimitKB = 10 * 1024;
  const groupLimitBytes = groupLimitKB * 1024;
  const globalLimitKB = groupLimitKB * 5;
  const globalLimitBytes = globalLimitKB * 1024;

  info("Setting limits");

  setGlobalLimit(globalLimitKB);

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Filling origins");

  await fillOrigin(getPrincipal("https://foo1.example1.com"), 100);
  await fillOrigin(getPrincipal("https://foo2.example1.com"), 200);
  await fillOrigin(getPrincipal("https://foo1.example2.com"), 300);
  await fillOrigin(getPrincipal("https://foo2.example2.com"), 400);

  info("Verifying origin estimations");

  await verifyOriginEstimation(
    getPrincipal("https://foo1.example1.com"),
    300,
    groupLimitBytes
  );
  await verifyOriginEstimation(
    getPrincipal("https://foo2.example1.com"),
    300,
    groupLimitBytes
  );
  await verifyOriginEstimation(
    getPrincipal("https://foo1.example2.com"),
    700,
    groupLimitBytes
  );
  await verifyOriginEstimation(
    getPrincipal("https://foo2.example2.com"),
    700,
    groupLimitBytes
  );

  info("Persisting origin");

  request = persist(getPrincipal("https://foo2.example2.com"));
  await requestFinished(request);

  info("Verifying origin estimation");

  await verifyOriginEstimation(
    getPrincipal("https://foo2.example2.com"),
    1000,
    globalLimitBytes
  );

  finishTest();
}
