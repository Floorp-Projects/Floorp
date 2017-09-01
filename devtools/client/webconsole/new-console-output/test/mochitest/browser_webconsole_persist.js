/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that message persistence works - bug 705921 / bug 1307881

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/mochitest/test-console.html";

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.webconsole.persistlog");
});

add_task(async function () {
  info("Testing that messages disappear on a refresh if logs aren't persisted");
  let hud = await openNewTabAndConsole(TEST_URI);

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    content.wrappedJSObject.doLogs(5);
  });
  await waitFor(() => findMessages(hud, "").length === 5);
  ok(true, "Messages showed up initially");

  await refreshTab();
  await waitFor(() => findMessages(hud, "").length === 0);
  ok(true, "Messages disappeared");

  await closeToolbox();
});

add_task(async function () {
  info("Testing that messages persist on a refresh if logs are persisted");

  let hud = await openNewTabAndConsole(TEST_URI);

  hud.ui.outputNode.querySelector(".webconsole-filterbar-primary .filter-checkbox")
    .click();

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    content.wrappedJSObject.doLogs(5);
  });
  await waitFor(() => findMessages(hud, "").length === 5);
  ok(true, "Messages showed up initially");

  await refreshTab();
  await waitFor(() => findMessages(hud, "").length === 6);

  ok(findMessage(hud, "Navigated"), "Navigated message appeared");
});
