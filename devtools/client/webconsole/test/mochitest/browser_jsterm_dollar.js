/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that using `$` and `$$` in jsterm call the global content functions
// if they are defined. See Bug 621644.

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-jsterm-dollar.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await test$(hud);
  await test$$(hud);
});

async function test$(hud) {
  hud.jsterm.clearOutput();
  const msg = await hud.jsterm.execute("$(document.body)");
  ok(msg.textContent.includes("<p>"), "jsterm output is correct for $()");
}

async function test$$(hud) {
  hud.jsterm.clearOutput();
  hud.jsterm.setInputValue();
  const msg = await hud.jsterm.execute("$$(document)");
  ok(msg.textContent.includes("621644"), "jsterm output is correct for $$()");
}
