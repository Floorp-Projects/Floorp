/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the inspect() jsterm helper function works.

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/test/browser/" +
  "test-simple-function.html";

add_task(async function() {
  let hud = await openNewTabAndConsole(TEST_URI);
  const toolbox = gDevTools.getToolbox(hud.target);

  await testInspectingElement(hud, toolbox);
  await testInspectingFunction(hud, toolbox);

  hud = await openNewTabAndConsole(TEST_URI);
  await testInspectingWindow(hud);
  await testInspectIngPrimitive(hud);
});

async function testInspectingElement(hud, toolbox) {
  info("Test `inspect(el)`");
  execute(hud, "inspect(document.querySelector('p'))");
  await waitForSelectedElementInInspector(toolbox, "p");
  ok(true, "inspected element is now selected in the inspector");
}

async function testInspectingFunction(hud, toolbox) {
  info("Test `inspect(test)`");
  execute(hud, "inspect(test)");

  await waitFor(() => {
    const dbg = toolbox.getPanel("jsdebugger");
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

async function testInspectingWindow(hud) {
  info("Test `inspect(window)`");
  // Add a global value so we can check it later.
  execute(hud, "testProp = 'testValue'");
  execute(hud, "inspect(window)");

  const objectInspectors = await waitFor(() => {
    const message = findInspectResultMessage(hud.ui.outputNode, 1);
    if (!message) {
      return false;
    }

    const treeEls = message.querySelectorAll(".tree");
    if (treeEls.length == 0) {
      return false;
    }
    return [...treeEls];
  });

  is(
    objectInspectors.length,
    1,
    "There is the expected number of object inspectors"
  );

  const [windowOi] = objectInspectors;
  let windowOiNodes = windowOi.querySelectorAll(".node");

  // The tree can be collapsed since the properties are fetched asynchronously.
  if (windowOiNodes.length === 1) {
    // If this is the case, we wait for the properties to be fetched and displayed.
    await waitForNodeMutation(windowOi, {
      childList: true,
    });
    windowOiNodes = windowOi.querySelectorAll(".node");
  }

  const propertiesNodes = [...windowOi.querySelectorAll(".object-label")];
  const testPropertyLabelNode = propertiesNodes.find(
    el => el.textContent === "testProp"
  );
  ok(
    testPropertyLabelNode,
    "The testProp property label is displayed as expected"
  );

  const testPropertyValueNode = testPropertyLabelNode
    .closest(".node")
    .querySelector(".objectBox");
  is(
    testPropertyValueNode.textContent,
    '"testValue"',
    "The testProp property value is displayed as expected"
  );
}

async function testInspectIngPrimitive(hud) {
  info("Test `inspect(1)`");
  execute(hud, "inspect(1)");

  const inspectPrimitiveNode = await waitFor(() =>
    findInspectResultMessage(hud.ui.outputNode, 2)
  );
  is(
    inspectPrimitiveNode.textContent,
    1,
    "The primitive is displayed as expected"
  );
}

function findInspectResultMessage(node, index) {
  return node.querySelectorAll(".message.result")[index];
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
