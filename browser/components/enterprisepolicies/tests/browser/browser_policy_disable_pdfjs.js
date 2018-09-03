/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {PdfJs} = ChromeUtils.import("resource://pdf.js/PdfJs.jsm", {});

add_task(async function test_disable_pdfjs() {
  is(PdfJs.enabled, true, "PDFjs should be enabled before policy runs");

  await setupPolicyEngineWithJson({
    "policies": {
      "DisableBuiltinPDFViewer": true,
    },
  });

  is(PdfJs.enabled, false, "PDFjs should be disabled after policy runs");
});
