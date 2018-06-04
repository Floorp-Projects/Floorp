/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* We are loading:
a script that is allowed by the CSP header but not by the CSPRO header
an image which is allowed by the CSPRO header but not by the CSP header.

So we expect a warning (image has been blocked) and a report
 (script should not load and was reported)

The expected console messages in the constants CSP_VIOLATION_MSG and
CSP_REPORT_MSG are confirmed to be found in the console messages.

See Bug 1010953.
*/

"use strict";

const TEST_URI = "data:text/html;charset=utf8,Web Console CSP report only test";
const TEST_VIOLATION = "http://example.com/browser/devtools/client/webconsole/" +
                       "test/mochitest/test-cspro.html";
const CSP_VIOLATION_MSG =
  "Content Security Policy: The page\u2019s settings blocked the loading of a resource " +
  "at http://some.example.com/cspro.png (\u201cimg-src\u201d).";
const CSP_REPORT_MSG =
  "Content Security Policy: The page\u2019s settings observed the loading of a " +
  "resource at http://some.example.com/cspro.js " +
  "(\u201cscript-src\u201d). A CSP report is being sent.";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const onCspViolationMessage = waitForMessage(hud, CSP_VIOLATION_MSG, ".message.error");
  const onCspReportMessage = waitForMessage(hud, CSP_REPORT_MSG, ".message.error");

  info("Load a page with CSP warnings.");
  loadDocument(TEST_VIOLATION);

  await onCspViolationMessage;
  await onCspReportMessage;
  ok(true, "Confirmed that CSP and CSP-Report-Only log different messages to console");

  hud.ui.clearOutput(true);
});
