/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TESTROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://mochi.test:8888/"
);

/**
 * Check that if we open a PDF with octet-stream mimetype, it can load
 * PDF.js .
 */
add_task(async function test_octet_stream_opens_pdfjs() {
  await SpecialPowers.pushPrefEnv({ set: [["pdfjs.handleOctetStream", true]] });
  // Get a ref to the pdf we want to open.
  let pdfURL = TESTROOT + "file_pdfjs_object_stream.pdf";

  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let handlerInfo = mimeService.getFromTypeAndExtension(
    "application/pdf",
    "pdf"
  );

  // Make sure pdf.js is the default handler.
  is(
    handlerInfo.alwaysAskBeforeHandling,
    false,
    "pdf handler defaults to always-ask is false"
  );
  is(
    handlerInfo.preferredAction,
    Ci.nsIHandlerInfo.handleInternally,
    "pdf handler defaults to internal"
  );

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(newTabBrowser) {
      await waitForPdfJS(newTabBrowser, pdfURL);
      is(newTabBrowser.currentURI.spec, pdfURL, "Should load pdfjs");
    }
  );
});
