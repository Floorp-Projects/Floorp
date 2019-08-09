/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that stack traces are shown when primitive values are thrown instead of
// error objects.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/" +
  "test-primitive-stacktrace.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await checkMessageStack(hud, "hello", [14, 10, 7]);
  await checkMessageStack(hud, "1,2,3", [20]);
});
