"use strict";

const TEST_PATH_HTTP = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

const TIMEOUT_PAGE_URI_HTTP =
  TEST_PATH_HTTP + "file_httpsfirst_timeout_server.sjs";

async function runPrefTest(aURI, aDesc, aAssertURLStartsWith) {
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    const loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    BrowserTestUtils.startLoadingURIString(browser, aURI);
    await loaded;

    await ContentTask.spawn(
      browser,
      { aDesc, aAssertURLStartsWith },
      function ({ aDesc, aAssertURLStartsWith }) {
        ok(
          content.document.location.href.startsWith(aAssertURLStartsWith),
          aDesc
        );
      }
    );
  });
}

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", false]],
  });
  Services.fog.testResetFOG();

  await runPrefTest(
    "http://example.com",
    "HTTPS-First disabled; Should not upgrade",
    "http://"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", true]],
  });

  for (const key of [
    "upgraded",
    "upgradedSchemeless",
    "downgraded",
    "downgradedSchemeless",
    "downgradedOnTimer",
    "downgradedOnTimerSchemeless",
    "downgradeTime",
    "downgradeTimeSchemeless",
  ]) {
    is(
      Glean.httpsfirst[key].testGetValue(),
      null,
      `No telemetry should have been recorded yet for ${key}`
    );
  }

  await runPrefTest(
    "http://example.com",
    "Should upgrade upgradeable website",
    "https://"
  );

  await runPrefTest(
    "http://httpsfirst.com",
    "Should downgrade after error.",
    "http://"
  );

  await runPrefTest(
    "http://httpsfirst.com/?https://httpsfirst.com",
    "Should downgrade after error and leave query params untouched.",
    "http://httpsfirst.com/?https://httpsfirst.com"
  );

  await runPrefTest(
    "http://domain.does.not.exist",
    "Should not downgrade on dnsNotFound error.",
    "https://"
  );

  await runPrefTest(
    TIMEOUT_PAGE_URI_HTTP,
    "Should downgrade after timeout.",
    "http://"
  );

  await runPrefTest(
    "http://invalid.example.com",
    "Should downgrade non-reachable site.",
    "http://"
  );

  info("Checking expected telemetry");
  is(Glean.httpsfirst.upgraded.testGetValue(), 1);
  is(Glean.httpsfirst.upgradedSchemeless.testGetValue(), null);
  is(Glean.httpsfirst.downgraded.testGetValue(), 3);
  is(Glean.httpsfirst.downgradedSchemeless.testGetValue(), null);
  is(Glean.httpsfirst.downgradedOnTimer.testGetValue().numerator, 1);
  is(Glean.httpsfirst.downgradedOnTimerSchemeless.testGetValue(), null);
  const downgradeSeconds =
    Glean.httpsfirst.downgradeTime.testGetValue().sum / 1_000_000_000;
  Assert.less(
    downgradeSeconds,
    10,
    "Summed downgrade time should be below 10 seconds"
  );
  is(null, Glean.httpsfirst.downgradeTimeSchemeless.testGetValue());
});
