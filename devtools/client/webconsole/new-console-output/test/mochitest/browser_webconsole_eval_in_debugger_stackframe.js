/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that makes sure web console eval happens in the user-selected stackframe
// from the js debugger.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "new-console-output/test/mochitest/test-eval-in-stackframe.html";

add_task(async function () {
  // Force the old debugger UI since it's directly used (see Bug 1301705).
  await pushPref("devtools.debugger.new-debugger-frontend", false);

  info("open the console");
  const hud = await openNewTabAndConsole(TEST_URI);
  const {jsterm} = hud;

  info("Check `foo` value");
  let onResultMessage = waitForMessage(hud, "globalFooBug783499");
  jsterm.execute("foo");
  await onResultMessage;
  ok(true, "|foo| value is correct");

  info("Assign and check `foo2` value");
  onResultMessage = waitForMessage(hud, "newFoo");
  jsterm.execute("foo2 = 'newFoo'; window.foo2");
  await onResultMessage;
  ok(true, "'newFoo' is displayed after adding `foo2`");

  info("Open the debugger and then select the console again");
  const {panel} = await openDebugger();
  const {activeThread, StackFrames: stackFrames} = panel.panelWin.DebuggerController;

  await openConsole();

  info("Check `foo + foo2` value");
  onResultMessage = waitForMessage(hud, "globalFooBug783499newFoo");
  jsterm.execute("foo + foo2");
  await onResultMessage;

  info("Select the debugger again");
  await openDebugger();

  const onFirstCallFramesAdded = activeThread.addOneTimeListener("framesadded");
  // firstCall calls secondCall, which has a debugger statement, so we'll be paused.
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    content.wrappedJSObject.firstCall();
  });
  await onFirstCallFramesAdded;

  info("frames added, select the console again");
  await openConsole();

  info("Check `foo + foo2` value when paused");
  onResultMessage = waitForMessage(hud, "globalFooBug783499foo2SecondCall");
  jsterm.execute("foo + foo2");
  ok(true, "`foo + foo2` from `secondCall()`");

  info("select the debugger and select the frame (1)");
  await openDebugger();
  stackFrames.selectFrame(1);
  await openConsole();

  info("Check `foo + foo2 + foo3` value when paused on a given frame");
  onResultMessage = waitForMessage(hud, "fooFirstCallnewFoofoo3FirstCall");
  jsterm.execute("foo + foo2 + foo3");
  await onResultMessage;
  ok(true, "`foo + foo2 + foo3` from `firstCall()`");

  onResultMessage = waitForMessage(hud, "abbabug783499");
  jsterm.execute("foo = 'abba'; foo3 = 'bug783499'; foo + foo3");
  await onResultMessage;
  ok(true, "`foo + foo3` updated in `firstCall()`");

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    is(content.wrappedJSObject.foo, "globalFooBug783499", "`foo` in content window");
    is(content.wrappedJSObject.foo2, "newFoo", "`foo2` in content window");
    ok(!content.wrappedJSObject.foo3, "`foo3` was not added to the content window");
  });
});
