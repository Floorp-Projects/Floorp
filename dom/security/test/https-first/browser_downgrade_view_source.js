// This test ensures that view-source:https falls back to view-source:http
"use strict";

async function runTest(desc, url, expectedURI, excpectedContent) {
  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    let loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    BrowserTestUtils.loadURI(browser, url);
    await loaded;

    await SpecialPowers.spawn(
      browser,
      [desc, expectedURI, excpectedContent],
      async function(desc, expectedURI, excpectedContent) {
        let loadedURI = content.document.documentURI;
        is(loadedURI, expectedURI, desc);
        let loadedContent = content.document.body.textContent;
        is(loadedContent, excpectedContent, desc);
      }
    );
  });
}

add_task(async function() {
  requestLongerTimeout(2);

  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", true]],
  });

  await runTest(
    "URL with query 'downgrade' should be http:",
    "view-source:http://example.com/tests/dom/security/test/https-first/file_downgrade_view_source.sjs?downgrade",
    "view-source:http://example.com/tests/dom/security/test/https-first/file_downgrade_view_source.sjs?downgrade",
    "view-source:http://"
  );

  await runTest(
    "URL with query 'upgrade' should be https:",
    "view-source:http://example.com/tests/dom/security/test/https-first/file_downgrade_view_source.sjs?upgrade",
    "view-source:https://example.com/tests/dom/security/test/https-first/file_downgrade_view_source.sjs?upgrade",
    "view-source:https://"
  );
});
