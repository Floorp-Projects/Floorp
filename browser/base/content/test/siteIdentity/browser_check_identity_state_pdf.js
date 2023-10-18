/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that sites opened via the PDF viewer have the correct identity state.
 */

"use strict";

function testIdentityMode(uri, expectedState, message) {
  return BrowserTestUtils.withNewTab(uri, () => {
    is(getIdentityMode(), expectedState, message);
  });
}

/**
 * Test site identity state for PDFs served via file URI.
 */
add_task(async function test_pdf_fileURI() {
  let path = getTestFilePath("./file_pdf.pdf");
  info("path:" + path);

  await testIdentityMode(
    path,
    "localResource",
    "Identity should be localResource for a PDF served via file URI"
  );
});

/**
 * Test site identity state for PDFs served via blob URI.
 */
add_task(async function test_pdf_blobURI() {
  let uri =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.com"
    ) + "file_pdf_blob.html";

  await BrowserTestUtils.withNewTab(uri, async browser => {
    let newTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

    await BrowserTestUtils.synthesizeMouseAtCenter("a", {}, browser);
    await newTabOpened;

    is(
      getIdentityMode(),
      "localResource",
      "Identity should be localResource for a PDF served via blob URI"
    );

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

/**
 * Test site identity state for PDFs served via HTTP.
 */
add_task(async function test_pdf_http() {
  let expectedIdentity = Services.prefs.getBoolPref(
    "security.insecure_connection_text.enabled"
  )
    ? "notSecure notSecureText"
    : "notSecure";
  const PDF_URI_NOSCHEME =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "example.com"
    ) + "file_pdf.pdf";

  await testIdentityMode(
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://" + PDF_URI_NOSCHEME,
    expectedIdentity,
    `Identity should be ${expectedIdentity} for a PDF served via HTTP.`
  );
  await testIdentityMode(
    "https://" + PDF_URI_NOSCHEME,
    "verifiedDomain",
    "Identity should be verifiedDomain for a PDF served via HTTPS."
  );
});
