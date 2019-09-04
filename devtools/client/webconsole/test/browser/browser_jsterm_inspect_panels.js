/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the inspect() jsterm helper function works.

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/test/browser/" +
  "test-simple-function.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await testInspectingElement(hud);
  await testInspectingFunction(hud);
});

async function testInspectingElement(hud) {
  info("Test `inspect(el)`");
  execute(hud, "inspect(document.querySelector('p'))");
  await waitForSelectedElementInInspector(hud.toolbox, "p");
  ok(true, "inspected element is now selected in the inspector");
}

async function testInspectingFunction(hud) {
  info("Test `inspect(test)`");
  execute(hud, "inspect(test)");

  await waitFor(() => {
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

    return (
      selectedLocation.sourceId.includes("test-simple-function.js") &&
      selectedLocation.line == 3
    );
  });

  ok(true, "inspected function is now selected in the debugger");
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
