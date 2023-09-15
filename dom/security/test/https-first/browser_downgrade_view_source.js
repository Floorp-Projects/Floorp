// This test ensures that view-source:https falls back to view-source:http
"use strict";

const TEST_PATH_HTTP = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

const TEST_PATH_HTTPS = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

async function runTest(desc, url, expectedURI, excpectedContent) {
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    let loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    BrowserTestUtils.startLoadingURIString(browser, url);
    await loaded;

    await SpecialPowers.spawn(
      browser,
      [desc, expectedURI, excpectedContent],
      async function (desc, expectedURI, excpectedContent) {
        let loadedURI = content.document.documentURI;
        is(loadedURI, expectedURI, desc);
        let loadedContent = content.document.body.textContent;
        is(loadedContent, excpectedContent, desc);
      }
    );
  });
}

add_task(async function () {
  requestLongerTimeout(2);

  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", true]],
  });

  await runTest(
    "URL with query 'downgrade' should be http:",
    `view-source:${TEST_PATH_HTTP}/file_downgrade_view_source.sjs?downgrade`,
    `view-source:${TEST_PATH_HTTP}/file_downgrade_view_source.sjs?downgrade`,
    "view-source:http://"
  );

  await runTest(
    "URL with query 'downgrade' should be http and leave query params untouched:",
    `view-source:${TEST_PATH_HTTP}/file_downgrade_view_source.sjs?downgrade&https://httpsfirst.com`,
    `view-source:${TEST_PATH_HTTP}/file_downgrade_view_source.sjs?downgrade&https://httpsfirst.com`,
    "view-source:http://"
  );

  await runTest(
    "URL with query 'upgrade' should be https:",
    `view-source:${TEST_PATH_HTTP}/file_downgrade_view_source.sjs?upgrade`,
    `view-source:${TEST_PATH_HTTPS}/file_downgrade_view_source.sjs?upgrade`,
    "view-source:https://"
  );

  await runTest(
    "URL with query 'upgrade' should be https:",
    `view-source:${TEST_PATH_HTTPS}/file_downgrade_view_source.sjs?upgrade`,
    `view-source:${TEST_PATH_HTTPS}/file_downgrade_view_source.sjs?upgrade`,
    "view-source:https://"
  );

  await runTest(
    "URL with query 'upgrade' should be https and leave query params untouched:",
    `view-source:${TEST_PATH_HTTP}/file_downgrade_view_source.sjs?upgrade&https://httpsfirst.com`,
    `view-source:${TEST_PATH_HTTPS}/file_downgrade_view_source.sjs?upgrade&https://httpsfirst.com`,
    "view-source:https://"
  );

  await runTest(
    "URL with query 'upgrade' should be https and leave query params untouched:",
    `view-source:${TEST_PATH_HTTPS}/file_downgrade_view_source.sjs?upgrade&https://httpsfirst.com`,
    `view-source:${TEST_PATH_HTTPS}/file_downgrade_view_source.sjs?upgrade&https://httpsfirst.com`,
    "view-source:https://"
  );
});
