// Bug 1659505 - Https-Only: CORS and MixedContent tests
// https://bugzilla.mozilla.org/bug/1659505
"use strict";

// > How does this test work?
// We open a page, that makes two fetch-requests to example.com (same-origin)
// and example.org (cross-origin). When both fetch-calls have either failed or
// succeeded, the site dispatches an event with the results.

add_task(async function () {
  // HTTPS-Only Mode disabled
  await runTest({
    description: "Load site with HTTP and HOM disabled",
    topLevelScheme: "http",

    expectedSameOrigin: "success", // ok
    expectedCrossOrigin: "error", // CORS
  });
  await runTest({
    description: "Load site with HTTPS and HOM disabled",
    topLevelScheme: "https",

    expectedSameOrigin: "error", // Mixed Content
    expectedCrossOrigin: "error", // Mixed Content
  });

  // HTTPS-Only Mode disabled and MixedContent blocker disabled
  await SpecialPowers.pushPrefEnv({
    set: [["security.mixed_content.block_active_content", false]],
  });
  await runTest({
    description: "Load site with HTTPS; HOM and MixedContent blocker disabled",
    topLevelScheme: "https",

    expectedSameOrigin: "error", // CORS
    expectedCrossOrigin: "error", // CORS
  });
  await SpecialPowers.popPrefEnv();

  // HTTPS-Only Mode enabled, no exception
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });
  await runTest({
    description: "Load site with HTTP and HOM enabled",
    topLevelScheme: "http",

    expectedSameOrigin: "success", // ok
    expectedCrossOrigin: "error", // CORS
  });

  // HTTPS-Only enabled, with exception
  await SpecialPowers.pushPermissions([
    {
      type: "https-only-load-insecure",
      allow: true,
      context: "http://example.com",
    },
  ]);

  await runTest({
    description: "Load site with HTTP, HOM enabled but site exempt",
    topLevelScheme: "http",

    expectedSameOrigin: "success", // ok
    expectedCrossOrigin: "error", // CORS
  });

  await SpecialPowers.popPermissions();
  await SpecialPowers.pushPermissions([
    {
      type: "https-only-load-insecure",
      allow: true,
      context: "https://example.com",
    },
  ]);
  await runTest({
    description: "Load site with HTTPS, HOM enabled but site exempt",
    topLevelScheme: "https",

    expectedSameOrigin: "error", // Mixed Content
    expectedCrossOrigin: "error", // Mixed Content
  });

  // Remove permission again (has to be done manually for some reason?)
  await SpecialPowers.popPermissions();
});

const SERVER_URL = scheme =>
  `${scheme}://example.com/browser/dom/security/test/https-only/file_cors_mixedcontent.html`;

async function runTest(test) {
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    let loaded = BrowserTestUtils.browserLoaded(browser);

    BrowserTestUtils.startLoadingURIString(
      browser,
      SERVER_URL(test.topLevelScheme)
    );

    await loaded;

    // eslint-disable-next-line no-shadow
    await SpecialPowers.spawn(browser, [test], async function (test) {
      const promise = new Promise(resolve => {
        content.addEventListener("FetchEnded", resolve, {
          once: true,
        });
      });

      content.dispatchEvent(new content.Event("StartFetch"));

      const { detail } = await promise;

      is(
        detail.comResult,
        test.expectedSameOrigin,
        `${test.description} (same-origin)`
      );
      is(
        detail.orgResult,
        test.expectedCrossOrigin,
        `${test.description} (cross-origin)`
      );
    });
  });
}
