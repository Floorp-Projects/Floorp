/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that scripts that override properties of the global object, like
 * toString, don't break the debugger. The test page used to cause the debugger
 * to throw when trying to attach to the thread actor.
 */

"use strict";

add_task(async function() {
  const dbg = await initDebugger("doc_global-method-override.html");
  ok(dbg, "Should have a debugger available.");
  is(dbg.toolbox.threadClient.state, "attached", "Debugger should be attached.");
});
