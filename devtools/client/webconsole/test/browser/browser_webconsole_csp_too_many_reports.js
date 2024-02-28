/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This tests loads a page that triggers so many CSP reports that they throttled
 * and a console error is logged.
 */

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html>Web Console CSP too many reports test";
const TEST_VIOLATIONS =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-csp-many-errors.html";

const bundle = Services.strings.createBundle(
  "chrome://global/locale/security/csp.properties"
);
const CSP_VIOLATION_MSG = bundle.formatStringFromName(
  "CSPInlineStyleViolation",
  ["style-src 'none'", "style-src-attr"]
);
const CSP_TOO_MANY_REPORTS_MSG = bundle.formatStringFromName(
  "tooManyReports",
  []
);

add_task(async function () {
  // Reduce the limit to reduce the log spam.
  await SpecialPowers.pushPrefEnv({
    set: [["security.csp.reporting.limit.count", 10]],
  });

  const hud = await openNewTabAndConsole(TEST_URI);

  const onCspViolationMessage = waitForMessageByType(
    hud,
    CSP_VIOLATION_MSG,
    ".error"
  ).then(() => info("Got violation message."));

  const onCspTooManyReportsMessage = waitForMessageByType(
    hud,
    CSP_TOO_MANY_REPORTS_MSG,
    ".error"
  ).then(() => info("Got too many reports message."));

  info("Load a page with CSP warnings.");
  await navigateTo(TEST_VIOLATIONS);

  info("Waiting for console messages.");
  await Promise.all([onCspViolationMessage, onCspTooManyReportsMessage]);
  ok(true, "Got error about too many reports");

  await clearOutput(hud);
});
