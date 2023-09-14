"use strict";

const kTestPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const kTestXFrameOptionsURI = kTestPath + "file_framing_error_pages_xfo.html";
const kTestXFrameOptionsURIFrame =
  kTestPath + "file_framing_error_pages.sjs?xfo";

const kTestFrameAncestorsURI = kTestPath + "file_framing_error_pages_csp.html";
const kTestFrameAncestorsURIFrame =
  kTestPath + "file_framing_error_pages.sjs?csp";

add_task(async function open_test_xfo_error_page() {
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    let loaded = BrowserTestUtils.browserLoaded(
      browser,
      true,
      kTestXFrameOptionsURIFrame,
      true
    );
    BrowserTestUtils.startLoadingURIString(browser, kTestXFrameOptionsURI);
    await loaded;

    await SpecialPowers.spawn(browser, [], async function () {
      const iframeDoc =
        content.document.getElementById("testframe").contentDocument;
      let errorPage = iframeDoc.body.innerHTML;
      ok(errorPage.includes("csp-xfo-error-title"), "xfo error page correct");
    });
  });
});

add_task(async function open_test_csp_frame_ancestor_error_page() {
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    let loaded = BrowserTestUtils.browserLoaded(
      browser,
      true,
      kTestFrameAncestorsURIFrame,
      true
    );
    BrowserTestUtils.startLoadingURIString(browser, kTestFrameAncestorsURI);
    await loaded;

    await SpecialPowers.spawn(browser, [], async function () {
      const iframeDoc =
        content.document.getElementById("testframe").contentDocument;
      let errorPage = iframeDoc.body.innerHTML;
      ok(errorPage.includes("csp-xfo-error-title"), "csp error page correct");
    });
  });
});
