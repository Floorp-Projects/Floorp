// Bug 1658264 - Https-Only: HTTPS-Only and iFrames
// https://bugzilla.mozilla.org/show_bug.cgi?id=1658264
"use strict";

// > How does this test work?
// We're sending a request to file_iframe_test.sjs with various
// browser-configurations. The sjs-file returns a website with two iFrames
// loading the same sjs-file again. One iFrame is same origin (example.com) and
// the other cross-origin (example.org) Each request gets saved in a semicolon
// seperated list of strings. The sjs-file gets initialized with the
// query-string "setup" and the result string can be polled with "results". Each
// string has this format: {top/com/org}-{queryString}-{scheme}. In the end
// we're just checking if all expected requests were recorded and had the
// correct scheme. Requests that are meant to fail should explicitly not be
// contained in the list of results.

// The test loads all tabs and evaluates when all have finished loading
// it may take quite a long time.
// This requires more twice as much as the default 45 seconds per test:
requestLongerTimeout(2);
SimpleTest.requestCompleteLog();

add_task(async function () {
  await setup();

  // Using this variable to parallelize and collect tests
  let testSet = [];

  /*
   * HTTPS-Only Mode disabled
   */
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", false]],
  });

  // Top-Level scheme: HTTP
  // NOTE(freddyb): Test case temporarily disabled. See bug 1735565
  /*testSet.push(
    runTest({
      queryString: "test1.1",
      topLevelScheme: "http",

      expectedTopLevel: "http",
      expectedSameOrigin: "http",
      expectedCrossOrigin: "http",
    })
  );*/
  // Top-Level scheme: HTTPS
  testSet.push(
    runTest({
      queryString: "test1.2",
      topLevelScheme: "https",

      expectedTopLevel: "https",
      expectedSameOrigin: "fail",
      expectedCrossOrigin: "fail",
    })
  );

  await Promise.all(testSet);
  testSet = [];
  /*
   * HTTPS-Only Mode enabled, no exception
   */
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  // Top-Level scheme: HTTP
  testSet.push(
    runTest({
      queryString: "test2.1",
      topLevelScheme: "http",

      expectedTopLevel: "https",
      expectedSameOrigin: "https",
      expectedCrossOrigin: "https",
    })
  );
  // Top-Level scheme: HTTPS
  testSet.push(
    runTest({
      queryString: "test2.2",
      topLevelScheme: "https",

      expectedTopLevel: "https",
      expectedSameOrigin: "https",
      expectedCrossOrigin: "https",
    })
  );

  await Promise.all(testSet);
  testSet = [];

  /*
   * HTTPS-Only enabled, with exceptions
   * for http://example.org and http://example.com
   */
  // Exempting example.org (cross-site) should not affect anything
  await SpecialPowers.pushPermissions([
    {
      type: "https-only-load-insecure",
      allow: true,
      context: "http://example.org",
    },
  ]);
  await SpecialPowers.pushPermissions([
    {
      type: "https-only-load-insecure",
      allow: true,
      context: "http://example.com",
    },
  ]);

  // Top-Level scheme: HTTP
  await runTest({
    queryString: "test3.1",
    topLevelScheme: "http",

    expectedTopLevel: "http",
    expectedSameOrigin: "http",
    expectedCrossOrigin: "http",
  });

  await SpecialPowers.popPermissions();
  await SpecialPowers.pushPermissions([
    {
      type: "https-only-load-insecure",
      allow: true,
      context: "https://example.com",
    },
  ]);
  // Top-Level scheme: HTTPS
  await runTest({
    queryString: "test3.2",
    topLevelScheme: "https",

    expectedTopLevel: "https",
    expectedSameOrigin: "fail",
    expectedCrossOrigin: "fail",
  });

  // Remove permissions again (has to be done manually for some reason?)
  await SpecialPowers.popPermissions();
  await SpecialPowers.popPermissions();

  await evaluate();
});

const SERVER_URL = scheme =>
  `${scheme}://example.com/browser/dom/security/test/https-only/file_iframe_test.sjs?`;
let shouldContain = [];
let shouldNotContain = [];

async function setup() {
  info(`TEST-CASE-setup - A`);
  const response = await fetch(SERVER_URL("https") + "setup");
  info(`TEST-CASE-setup - B`);
  const txt = await response.text();
  info(`TEST-CASE-setup - C`);
  if (txt != "ok") {
    ok(false, "Failed to setup test server.");
    finish();
  }
}

async function evaluate() {
  info(`TEST-CASE-evaluate - A`);
  const response = await fetch(SERVER_URL("https") + "results");
  info(`TEST-CASE-evaluate - B`);
  const requestResults = (await response.text()).split(";");
  info(`TEST-CASE-evaluate - C`);

  shouldContain.map(str =>
    ok(requestResults.includes(str), `Results should contain '${str}'.`)
  );
  shouldNotContain.map(str =>
    ok(!requestResults.includes(str), `Results shouldn't contain '${str}'.`)
  );
}

async function runTest(test) {
  const queryString = test.queryString;
  info(`TEST-CASE-${test.queryString} - runTest BEGIN`);
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    let loaded = BrowserTestUtils.browserLoaded(
      browser,
      false, // includeSubFrames
      SERVER_URL(test.expectedTopLevel) + queryString,
      false // maybeErrorPage
    );
    BrowserTestUtils.startLoadingURIString(
      browser,
      SERVER_URL(test.topLevelScheme) + queryString
    );
    info(`TEST-CASE-${test.queryString} - Before 'await loaded'`);
    await loaded;
    info(`TEST-CASE-${test.queryString} - After 'await loaded'`);
  });
  info(`TEST-CASE-${test.queryString} - After 'await withNewTab'`);

  if (test.expectedTopLevel !== "fail") {
    shouldContain.push(`top-${queryString}-${test.expectedTopLevel}`);
  } else {
    shouldNotContain.push(`top-${queryString}-http`);
    shouldNotContain.push(`top-${queryString}-https`);
  }

  if (test.expectedSameOrigin !== "fail") {
    shouldContain.push(`com-${queryString}-${test.expectedSameOrigin}`);
  } else {
    shouldNotContain.push(`com-${queryString}-http`);
    shouldNotContain.push(`com-${queryString}-https`);
  }

  if (test.expectedCrossOrigin !== "fail") {
    shouldContain.push(`org-${queryString}-${test.expectedCrossOrigin}`);
  } else {
    shouldNotContain.push(`org-${queryString}-http`);
    shouldNotContain.push(`org-${queryString}-https`);
  }
  info(`TEST-CASE-${test.queryString} - runTest END`);
}
