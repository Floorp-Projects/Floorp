/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the $0 console helper works as intended. See Bug 653531.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,
<head>
  <title>Inspector Tree Selection Test</title>
</head>
<body>
  <div>
    <h1>Inspector Tree Selection Test</h1>
    <p>This is some example text</p>
    <p>${loremIpsum()}</p>
  </div>
  <div>
    <p>${loremIpsum()}</p>
  </div>
</body>`.replace("\n", "");

add_task(async function() {
  const toolbox = await openNewTabAndToolbox(TEST_URI, "inspector");
  const inspector = toolbox.getPanel("inspector");

  await registerTestActor(toolbox.target.client);
  const testActor = await getTestActor(toolbox);

  const onPickerStarted = inspector.toolbox.once("picker-started");
  inspector.toolbox.highlighterUtils.startPicker();
  await onPickerStarted;

  info("Picker mode started, now clicking on <h1> to select that node");
  const onPickerStopped = toolbox.once("picker-stopped");
  const onInspectorUpdated = inspector.once("inspector-updated");

  testActor.synthesizeMouse({
    selector: "h1",
    center: true,
    options: {}
  });

  await onPickerStopped;
  await onInspectorUpdated;

  info("Picker mode stopped, <h1> selected, now switching to the console");
  const hud = await openConsole();
  const {jsterm} = hud;

  hud.ui.clearOutput();

  const onEvaluationResult = waitForMessage(hud, "<h1>");
  jsterm.execute("$0");
  await onEvaluationResult;
  ok(true, "correct output for $0");

  hud.ui.clearOutput();

  const newH1Content = "newH1Content";
  const onAssignmentResult = waitForMessage(hud, "<h1>");
  jsterm.execute(`$0.textContent = "${newH1Content}";$0`);
  await onAssignmentResult;

  ok(true, "correct output for $0 after setting $0.textContent");
  const {textContent} = await testActor.getNodeInfo("h1");
  is(textContent, newH1Content, "node successfully updated");
});

function loremIpsum() {
  return `Lorem ipsum dolor sit amet, consectetur adipisicing
elit, sed do eiusmod tempor incididunt ut labore et dolore magna
aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco
laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure
dolor in reprehenderit in voluptate velit esse cillum dolore eu
fugiat nulla pariatur. Excepteur sint occaecat cupidatat non
proident, sunt in culpa qui officia deserunt mollit anim id est laborum.`
  .replace("\n", "");
}
