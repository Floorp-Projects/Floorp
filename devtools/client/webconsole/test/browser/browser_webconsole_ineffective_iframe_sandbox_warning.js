/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that warnings about ineffective iframe sandboxing are logged to the
// web console when necessary (and not otherwise). See Bug 752559.

"use strict";

requestLongerTimeout(2);

const TEST_PATH =
  "http://example.com/browser/devtools/client/webconsole/" + "test/browser/";
const TEST_URI_WARNING = `${TEST_PATH}test-ineffective-iframe-sandbox-warning0.html`;
const TEST_URI_NOWARNING = [
  `${TEST_PATH}test-ineffective-iframe-sandbox-warning1.html`,
  `${TEST_PATH}test-ineffective-iframe-sandbox-warning2.html`,
  `${TEST_PATH}test-ineffective-iframe-sandbox-warning3.html`,
  `${TEST_PATH}test-ineffective-iframe-sandbox-warning4.html`,
  `${TEST_PATH}test-ineffective-iframe-sandbox-warning5.html`,
];

const INEFFECTIVE_IFRAME_SANDBOXING_MSG =
  "An iframe which has both " +
  "allow-scripts and allow-same-origin for its sandbox attribute can remove " +
  "its sandboxing.";
const SENTINEL_MSG = "testing ineffective sandboxing message";

add_task(async function() {
  await testWarningMessageVisibility(TEST_URI_WARNING, true);

  for (const testUri of TEST_URI_NOWARNING) {
    await testWarningMessageVisibility(testUri, false);
  }
});

async function testWarningMessageVisibility(uri, visible) {
  const hud = await openNewTabAndConsole(uri, true);

  const sentinel = SENTINEL_MSG + Date.now();
  const onSentinelMessage = waitForMessage(hud, sentinel);

  ContentTask.spawn(gBrowser.selectedBrowser, sentinel, function(msg) {
    content.console.log(msg);
  });
  await onSentinelMessage;

  const warning = findMessage(
    hud,
    INEFFECTIVE_IFRAME_SANDBOXING_MSG,
    ".message.warn"
  );
  is(
    !!warning,
    visible,
    `The warning message is${visible ? "" : " not"} visible on ${uri}`
  );
}
