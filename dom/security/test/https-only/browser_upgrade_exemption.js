"use strict";

const PAGE_WITHOUT_SCHEME = "://example.com";

add_task(async function () {
  // Load a insecure page with HTTPS-Only and HTTPS-First disabled
  await runTest({
    loadScheme: "http",
    expectScheme: "http",
  });

  // Load a secure page with HTTPS-Only and HTTPS-First disabled
  await runTest({
    loadScheme: "https",
    expectScheme: "https",
  });

  // Load a exempted insecure page with HTTPS-Only and HTTPS-First disabled
  await runTest({
    exempt: true,
    loadScheme: "http",
    expectScheme: "http",
  });

  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  // Load a insecure page with HTTPS-Only enabled
  await runTest({
    loadScheme: "http",
    expectScheme: "https",
  });

  // Load a exempted insecure page with HTTPS-Only enabled
  await runTest({
    exempt: true,
    loadScheme: "http",
    expectScheme: "http",
  });

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", true]],
  });

  // Load a insecure page with HTTPS-First enabled
  await runTest({
    loadScheme: "http",
    expectScheme: "https",
  });

  // Load a exempted insecure page with HTTPS-First enabled
  await runTest({
    exempt: true,
    loadScheme: "http",
    expectScheme: "http",
  });
});

async function runTest(options) {
  const { exempt = false, loadScheme, expectScheme } = options;
  const page = loadScheme + PAGE_WITHOUT_SCHEME;

  if (exempt) {
    await SpecialPowers.pushPermissions([
      {
        type: "https-only-load-insecure",
        allow: true,
        context: page,
      },
    ]);
  }

  await BrowserTestUtils.withNewTab(page, async function (browser) {
    is(browser.currentURI.scheme, expectScheme, "Unexpected scheme");
    await SpecialPowers.popPermissions();
    await SpecialPowers.popPrefEnv();
  });
}
