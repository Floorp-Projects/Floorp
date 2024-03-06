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

  await runPrefTest(
    "http://example.com",
    "HTTPS-First disabled; Should not upgrade",
    "http://"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", true]],
  });

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
});
