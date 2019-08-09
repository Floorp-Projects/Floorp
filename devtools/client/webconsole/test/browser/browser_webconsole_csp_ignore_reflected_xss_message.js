/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that a file with an unsupported CSP directive ('reflected-xss filter')
// displays the appropriate message to the console. See Bug 1045902.

"use strict";

const EXPECTED_RESULT =
  "Not supporting directive \u2018reflected-xss\u2019. " +
  "Directive and values will be ignored.";
const TEST_FILE =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/" +
  "test_console_csp_ignore_reflected_xss_message.html";

const TEST_URI =
  "data:text/html;charset=utf8,Web Console CSP ignoring reflected XSS (bug 1045902)";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await loadDocument(TEST_FILE);

  await waitFor(() => findMessage(hud, EXPECTED_RESULT, ".message.warn"));
  ok(
    true,
    `CSP logs displayed in console when using "reflected-xss" directive`
  );

  Services.cache2.clear();
});
