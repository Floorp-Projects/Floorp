/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that switching to the inspector panel and not waiting for it to be fully
// loaded doesn't fail the test with unhandled rejected promises.

add_task(function*() {
  // At least one assertion is needed to avoid failing the test, but really,
  // what we're interested in is just having the test pass when switching to the
  // inspector.
  ok(true);

  yield addTab("data:text/html;charset=utf-8,test inspector destroy");

  info("Open the toolbox on the debugger panel");
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, "jsdebugger");

  info("Switch to the inspector panel and immediately end the test");
  let onInspectorSelected = toolbox.once("inspector-selected");
  toolbox.selectTool("inspector");
  let inspector = yield onInspectorSelected;
});
