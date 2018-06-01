/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI_REPLACED =
  "data:text/html;charset=utf8,<script>console = {log: () => ''}</script>";
const TEST_URI_NOT_REPLACED =
  "data:text/html;charset=utf8,<script>console.log('foo')</script>";

add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["devtools.webconsole.persistlog", true]
  ]});

  let hud = await openNewTabAndConsole(TEST_URI_NOT_REPLACED);

  await testWarningNotPresent(hud);
  await closeToolbox();

  await loadDocument(TEST_URI_REPLACED);

  const toolbox = await openToolboxForTab(gBrowser.selectedTab, "webconsole");
  hud = toolbox.getCurrentPanel().hud;
  await testWarningPresent(hud);
});

async function testWarningNotPresent(hud) {
  ok(!findMessage(hud, "logging API"), "no warning displayed");

  // Bug 862024: make sure the warning doesn't show after page reload.
  info("wait for the page to refresh and make sure the warning still isn't there");
  await refreshTab();
  await waitFor(() => {
    // We need to wait for 3 messages because there are two logs, plus the
    // navigation message since messages are persisted
    return findMessages(hud, "foo").length === 3;
  });

  ok(!findMessage(hud, "logging API"), "no warning displayed");
}

async function testWarningPresent(hud) {
  info("wait for the warning to show");
  await waitFor(() => findMessage(hud, "logging API"));

  info("reload the test page and wait for the warning to show");
  await refreshTab();
  await waitFor(() => {
    return findMessages(hud, "logging API").length === 2;
  });
}
