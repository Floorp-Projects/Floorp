"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function () {
  await BrowserTestUtils.withNewTab(
    TEST_PATH + "file_pdfjs_not_subject_to_csp.html",
    async function (browser) {
      let pdfPromise = BrowserTestUtils.waitForContentEvent(
        browser,
        "documentloaded",
        false,
        null,
        true
      );

      await ContentTask.spawn(browser, {}, async function () {
        let pdfButton = content.document.getElementById("pdfButton");
        pdfButton.click();
      });

      await pdfPromise;

      await ContentTask.spawn(browser, {}, async function () {
        let pdfFrame = content.document.getElementById("pdfFrame");
        // 1) Sanity that we have loaded the PDF using a blob
        ok(pdfFrame.src.startsWith("blob:"), "it's a blob URL");

        // 2) Ensure that the PDF has actually loaded
        ok(
          pdfFrame.contentDocument.querySelector("div#viewer"),
          "document content has viewer UI"
        );

        // 3) Ensure we have the correct CSP attached
        let cspJSON = pdfFrame.contentDocument.cspJSON;
        ok(cspJSON.includes("script-src"), "found script-src directive");
        ok(cspJSON.includes("allowPDF"), "found script-src nonce value");
      });
    }
  );
});
