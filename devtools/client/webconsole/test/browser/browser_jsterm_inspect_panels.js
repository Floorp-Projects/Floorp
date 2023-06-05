/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the inspect() jsterm helper function works.

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/test/browser/" +
  "test-simple-function.html";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  await testInspectingElement(hud);
  await testInspectingFunction(hud);
});

async function testInspectingElement(hud) {
  info("Test `inspect(el)`");
  execute(hud, "inspect(document.querySelector('p'))");
  await waitForSelectedElementInInspector(hud.toolbox, "p");
  ok(true, "inspected element is now selected in the inspector");

  info(
    "Test that inspect selects the node in the inspector in the split console as well"
  );
  const onSplitConsoleReady = hud.toolbox.once("split-console");
  EventUtils.sendKey("ESCAPE", hud.toolbox.win);
  await onSplitConsoleReady;

  execute(hud, "inspect(document.querySelector('body'))");
  await waitForSelectedElementInInspector(hud.toolbox, "body");
  ok(true, "the inspected element is selected in the inspector");
}

async function testInspectingFunction(hud) {
  info("Test `inspect(test)`");
  execute(hud, "inspect(test)");
  await waitFor(expectedSourceSelected("test-simple-function.js", 3));
  ok(true, "inspected function is now selected in the debugger");

  info("Test `inspect(test_mangled)`");
  execute(hud, "inspect(test_mangled)");
  await waitFor(expectedSourceSelected("test-mangled-function.js", 3, true));
  ok(true, "inspected source-mapped function is now selected in the debugger");

  info("Test `inspect(test_bound)`");
  execute(hud, "inspect(test_bound)");
  await waitFor(expectedSourceSelected("test-simple-function.js", 7));
  ok(true, "inspected bound target function is now selected in the debugger");

  function expectedSourceSelected(
    sourceFilename,
    sourceLine,
    isOriginalSource
  ) {
    return () => {
      const dbg = hud.toolbox.getPanel("jsdebugger");
      if (!dbg) {
        return false;
      }

      const selectedLocation = dbg._selectors.getSelectedLocation(
        dbg._getState()
      );

      if (!selectedLocation) {
        return false;
      }

      if (
        isOriginalSource &&
        !selectedLocation.source.id.includes("/originalSource-")
      ) {
        return false;
      }

      return (
        selectedLocation.source.id.includes(sourceFilename) &&
        selectedLocation.line == sourceLine
      );
    };
  }
}

async function waitForSelectedElementInInspector(toolbox, displayName) {
  return waitFor(() => {
    const inspector = toolbox.getPanel("inspector");
    if (!inspector) {
      return false;
    }

    const selection = inspector.selection;
    return (
      selection &&
      selection.nodeFront &&
      selection.nodeFront.displayName == displayName
    );
  });
}
