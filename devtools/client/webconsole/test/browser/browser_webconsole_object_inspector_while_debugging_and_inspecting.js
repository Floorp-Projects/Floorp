/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that makes sure web console eval works while the js debugger paused the
// page, and while the inspector is active. See bug 886137.

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-eval-in-stackframe.html";

add_task(async function() {
  // TODO: Remove this pref change when middleware for terminating requests
  // when closing a panel is implemented
  await pushPref("devtools.debugger.features.inline-preview", false);

  const hud = await openNewTabAndConsole(TEST_URI);

  info("Switch to the debugger");
  await openDebugger();

  info("Switch to the inspector");
  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  await gDevTools.showToolbox(target, "inspector");

  info("Call firstCall() and wait for the debugger statement to be reached.");
  const toolbox = gDevTools.getToolbox(target);
  const dbg = createDebuggerContext(toolbox);
  await pauseDebugger(dbg);

  info("Switch back to the console");
  await gDevTools.showToolbox(target, "webconsole");

  info("Test logging and inspecting objects while on a breakpoint.");
  const message = await executeAndWaitForMessage(
    hud,
    "fooObj",
    '{ testProp2: "testValue2" }',
    ".result"
  );

  const objectInspectors = [...message.node.querySelectorAll(".tree")];
  is(objectInspectors.length, 1, "There should be one object inspector");

  info("Expanding the array object inspector");
  const [oi] = objectInspectors;
  const onOiExpanded = waitFor(() => {
    return oi.querySelectorAll(".node").length === 3;
  });
  oi.querySelector(".arrow").click();
  await onOiExpanded;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "Object inspector expanded"
  );

  // The object inspector now looks like:
  // {...}
  // |  testProp2: "testValue2"
  // |  <prototype>: Object { ... }

  const oiNodes = oi.querySelectorAll(".node");
  is(oiNodes.length, 3, "There is the expected number of nodes in the tree");

  ok(oiNodes[0].textContent.includes(`{\u2026}`));
  ok(oiNodes[1].textContent.includes(`testProp2: "testValue2"`));
  ok(oiNodes[2].textContent.includes(`<prototype>: Object { \u2026 }`));
});
