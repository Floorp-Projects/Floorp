/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the inspect() jsterm helper function works.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>test inspect() command";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Test `inspect(window)`");
  // Add a global value so we can check it later.
  await executeAndWaitForMessage(
    hud,
    "testProp = 'testValue'",
    "testValue",
    ".result"
  );
  const { node: inspectWindowNode } = await executeAndWaitForMessage(
    hud,
    "inspect(window)",
    "Window",
    ".result"
  );

  const objectInspectors = [...inspectWindowNode.querySelectorAll(".tree")];
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

  /* Check that a primitive value can be inspected, too */
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
});

function findInspectResultMessage(node, index) {
  return node.querySelectorAll(".message.result")[index];
}
