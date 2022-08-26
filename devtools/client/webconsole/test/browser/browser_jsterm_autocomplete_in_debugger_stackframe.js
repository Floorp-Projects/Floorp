/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that makes sure web console autocomplete happens in the user-selected
// stackframe from the js debugger.

"use strict";
/* import-globals-from head.js*/

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-autocomplete-in-stackframe.html";

requestLongerTimeout(20);

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup: popup } = jsterm;

  const toolbox = await gDevTools.getToolboxForTab(gBrowser.selectedTab);

  const jstermComplete = value => setInputValueForAutocompletion(hud, value);

  // Test that document.title gives string methods. Native getters must execute.
  await jstermComplete("document.title.");

  const newItemsLabels = getAutocompletePopupLabels(popup);
  ok(!!newItemsLabels.length, "'document.title.' gave a list of suggestions");
  ok(newItemsLabels.includes("substr"), `results do contain "substr"`);
  ok(
    newItemsLabels.includes("toLowerCase"),
    `results do contain "toLowerCase"`
  );
  ok(newItemsLabels.includes("strike"), `results do contain "strike"`);

  // Test if 'foo' gives 'foo1' but not 'foo2' or 'foo3'
  await jstermComplete("foo");
  ok(
    hasExactPopupLabels(popup, ["foo1", "foo1Obj"]),
    `"foo" gave the expected suggestions`
  );

  // Test if 'foo1Obj.' gives 'prop1' and 'prop2'
  await jstermComplete("foo1Obj.");
  checkInputCompletionValue(hud, "prop1", "foo1Obj completion");
  ok(
    hasExactPopupLabels(popup, ["prop1", "prop2"]),
    `"foo1Obj." gave the expected suggestions`
  );

  // Test if 'foo1Obj.prop2.' gives 'prop21'
  await jstermComplete("foo1Obj.prop2.");
  ok(
    hasPopupLabel(popup, "prop21"),
    `"foo1Obj.prop2." gave the expected suggestions`
  );
  await closeAutocompletePopup(hud);

  info("Opening Debugger");
  await openDebugger();
  const dbg = createDebuggerContext(toolbox);

  info("Waiting for pause");
  await pauseDebugger(dbg);
  const stackFrames = dbg.selectors.getCallStackFrames();

  info("Opening Console again");
  await toolbox.selectTool("webconsole");

  // Test if 'foo' gives 'foo3' and 'foo1' but not 'foo2', since we are paused in
  // the `secondCall` function (called by `firstCall`, which we call in `pauseDebugger`).
  await jstermComplete("foo");
  ok(
    hasExactPopupLabels(popup, ["foo1", "foo1Obj", "foo3", "foo3Obj"]),
    `"foo." gave the expected suggestions`
  );

  await openDebugger();

  // Select the frame for the `firstCall` function.
  await selectFrame(dbg, stackFrames[1]);

  info("openConsole");
  await toolbox.selectTool("webconsole");

  // Test if 'foo' gives 'foo2' and 'foo1' but not 'foo3', since we are now in the
  // `firstCall` frame.
  await jstermComplete("foo");
  ok(
    hasExactPopupLabels(popup, ["foo1", "foo1Obj", "foo2", "foo2Obj"]),
    `"foo" gave the expected suggestions`
  );

  // Test if 'foo2Obj.' gives 'prop1'
  await jstermComplete("foo2Obj.");
  ok(hasPopupLabel(popup, "prop1"), `"foo2Obj." returns "prop1"`);

  // Test if 'foo2Obj.prop1.' gives 'prop11'
  await jstermComplete("foo2Obj.prop1.");
  ok(hasPopupLabel(popup, "prop11"), `"foo2Obj.prop1" returns "prop11"`);

  // Test if 'foo2Obj.prop1.prop11.' gives suggestions for a string,i.e. 'length'
  await jstermComplete("foo2Obj.prop1.prop11.");
  ok(hasPopupLabel(popup, "length"), `results do contain "length"`);

  // Test if 'foo2Obj[0].' throws no errors.
  await jstermComplete("foo2Obj[0].");
  is(getAutocompletePopupLabels(popup).length, 0, "no items for foo2Obj[0]");
});
