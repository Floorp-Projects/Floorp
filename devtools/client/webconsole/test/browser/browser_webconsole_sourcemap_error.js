/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a missing source map is reported.

const BASE =
  "http://example.com/browser/devtools/client/webconsole/" + "test/browser/";

add_task(async function() {
  for (const test of [
    "test-sourcemap-error-01.html",
    "test-sourcemap-error-02.html",
  ]) {
    const hud = await openNewTabAndConsole(BASE + test);

    const node = await waitFor(() => findMessage(hud, "here"));
    ok(node, "logged text is displayed in web console");

    const node2 = await waitFor(() => findMessage(hud, "Source map error"));
    ok(node2, "source map error is displayed in web console");
  }
});
