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

const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html>Web Console CSP report only test";
const TEST_VIOLATION =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-cspro.html";

const bundle = Services.strings.createBundle(
  "chrome://global/locale/security/csp.properties"
);
const CSP_VIOLATION_MSG = bundle.formatStringFromName("CSPGenericViolation", [
  "img-src 'self'",
  "http://some.example.com/cspro.png",
  "img-src",
]);
const CSP_REPORT_MSG = bundle.formatStringFromName("CSPROScriptViolation", [
  "script-src 'self'",
  "http://some.example.com/cspro.js",
  "script-src-elem",
]);

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const onCspViolationMessage = waitForMessageByType(
    hud,
    CSP_VIOLATION_MSG,
    ".error"
  );
  const onCspReportMessage = waitForMessageByType(
    hud,
    CSP_REPORT_MSG,
    ".error"
  );

  info("Load a page with CSP warnings.");
  await navigateTo(TEST_VIOLATION);

  await onCspViolationMessage;
  await onCspReportMessage;
  ok(
    true,
    "Confirmed that CSP and CSP-Report-Only log different messages to console"
  );

  await clearOutput(hud);
});
