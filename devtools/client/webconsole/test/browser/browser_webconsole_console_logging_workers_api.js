/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the basic console.log()-style APIs and filtering work for
// sharedWorkers

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console-workers.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const message = await waitFor(() =>
    findMessage(hud, "foo-bar-shared-worker")
  );
  is(
    message.querySelector(".message-body").textContent,
    `foo-bar-shared-worker Object { foo: "bar" }`,
    "log from SharedWorker is displayed as expected"
  );

  const onMessagesCleared = hud.ui.once("messages-cleared");
  hud.ui.clearOutput(true);
  await onMessagesCleared;
});
