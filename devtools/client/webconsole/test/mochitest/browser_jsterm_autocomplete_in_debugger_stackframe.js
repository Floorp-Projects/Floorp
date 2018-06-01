/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that makes sure web console autocomplete happens in the user-selected
// stackframe from the js debugger.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-autocomplete-in-stackframe.html";

add_task(async function() {
  // Force the old debugger UI since it's directly used (see Bug 1301705)
  await pushPref("devtools.debugger.new-debugger-frontend", false);

  const { jsterm } = await openNewTabAndConsole(TEST_URI);
  const {
    autocompletePopup: popup,
  } = jsterm;

  const target = TargetFactory.forTab(gBrowser.selectedTab);
  const toolbox = gDevTools.getToolbox(target);

  const jstermComplete = value => jstermSetValueAndComplete(jsterm, value);

  // Test that document.title gives string methods. Native getters must execute.
  await jstermComplete("document.title.");

  const newItemsLabels = getPopupLabels(popup);
  ok(newItemsLabels.length > 0, "'document.title.' gave a list of suggestions");
  ok(newItemsLabels.includes("substr"), `results do contain "substr"`);
  ok(newItemsLabels.includes("toLowerCase"), `results do contain "toLowerCase"`);
  ok(newItemsLabels.includes("strike"), `results do contain "strike"`);

  // Test if 'foo' gives 'foo1' but not 'foo2' or 'foo3'
  await jstermComplete("foo");
  is(getPopupLabels(popup).join("-"), "foo1Obj-foo1",
    `"foo" gave the expected suggestions`);

  // Test if 'foo1Obj.' gives 'prop1' and 'prop2'
  await jstermComplete("foo1Obj.");
  is(getPopupLabels(popup).join("-"), "prop2-prop1",
    `"foo1Obj." gave the expected suggestions`);

  // Test if 'foo1Obj.prop2.' gives 'prop21'
  await jstermComplete("foo1Obj.prop2.");
  ok(getPopupLabels(popup).includes("prop21"),
    `"foo1Obj.prop2." gave the expected suggestions`);

  info("Opening Debugger");
  const {panel} = await openDebugger();

  info("Waiting for pause");
  const stackFrames = await pauseDebugger(panel);

  info("Opening Console again");
  await toolbox.selectTool("webconsole");

  // Test if 'foo' gives 'foo3' and 'foo1' but not 'foo2', since we are paused in
  // the `secondCall` function (called by `firstCall`, which we call in `pauseDebugger`).
  await jstermComplete("foo");
  is(getPopupLabels(popup).join("-"), "foo3Obj-foo3-foo1Obj-foo1",
    `"foo" gave the expected suggestions`);

  await openDebugger();

  // Select the frame for the `firstCall` function.
  stackFrames.selectFrame(1);

  info("openConsole");
  await toolbox.selectTool("webconsole");

  // Test if 'foo' gives 'foo2' and 'foo1' but not 'foo3', since we are now in the
  // `firstCall` frame.
  await jstermComplete("foo");
  is(getPopupLabels(popup).join("-"), "foo2Obj-foo2-foo1Obj-foo1",
    `"foo" gave the expected suggestions`);

  // Test if 'foo2Obj.' gives 'prop1'
  await jstermComplete("foo2Obj.");
  ok(getPopupLabels(popup).includes("prop1"), `"foo2Obj." returns "prop1"`);

  // Test if 'foo2Obj.prop1.' gives 'prop11'
  await jstermComplete("foo2Obj.prop1.");
  ok(getPopupLabels(popup).includes("prop11"), `"foo2Obj.prop1" returns "prop11"`);

  // Test if 'foo2Obj.prop1.prop11.' gives suggestions for a string,i.e. 'length'
  await jstermComplete("foo2Obj.prop1.prop11.");
  ok(getPopupLabels(popup).includes("length"), `results do contain "length"`);

  // Test if 'foo2Obj[0].' throws no errors.
  await jstermComplete("foo2Obj[0].");
  is(getPopupLabels(popup).length, 0, "no items for foo2Obj[0]");
});

function getPopupLabels(popup) {
  return popup.getItems().map(item => item.label);
}

function pauseDebugger(debuggerPanel) {
  const debuggerWin = debuggerPanel.panelWin;
  const debuggerController = debuggerWin.DebuggerController;
  const thread = debuggerController.activeThread;

  return new Promise(resolve => {
    thread.addOneTimeListener("framesadded", () =>
      resolve(debuggerController.StackFrames));

    info("firstCall()");
    ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
      content.wrappedJSObject.firstCall();
    });
  });
}
