/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that throwing uncaught errors and primitive values in workers shows a
// stack in the console.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-error-worker.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await checkMessageStack(hud, "hello", [11, 3]);
  await checkMessageStack(hud, "there", [14, 3]);
  await checkMessageStack(hud, "dom", [16, 3]);
  await checkMessageStack(hud, "worker2", [6, 3]);
});
