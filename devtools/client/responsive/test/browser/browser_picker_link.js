/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that picking a link when using RDM does not trigger a navigation.
 * See Bug 1609199.
 */
const TEST_URI = `${URL_ROOT}doc_picker_link.html`;

addRDMTask(TEST_URI, async function ({ ui, manager }) {
  info("Open the rule-view and select the test node before opening RDM");
  const { inspector, toolbox } = await openRuleView();
  await selectNode("body", inspector);

  info("Open RDM");

  // XXX: Using toggleTouchSimulation waits for browser loaded, which is not
  // fired here?
  info("Toggle Touch simulation");
  const { document } = ui.toolWindow;
  const touchButton = document.getElementById("touch-simulation-button");
  const changed = once(ui, "touch-simulation-changed");
  touchButton.click();
  await changed;

  info("Waiting for element picker to become active.");
  await startPicker(toolbox, ui);

  info("Move mouse over the pick-target");
  await hoverElement(inspector, ui, ".picker-link", 15, 15);

  // Add a listener on the "navigate" event.
  let hasNavigated = false;
  const { onDomCompleteResource } =
    await waitForNextTopLevelDomCompleteResource(toolbox.commands);

  onDomCompleteResource.then(() => {
    hasNavigated = true;
  });

  info("Click and pick the link");
  await pickElement(inspector, ui, ".picker-link");

  // Wait until page to start navigation.
  await wait(2000);
  ok(
    !hasNavigated,
    "The page should not have navigated when picking the <a> element"
  );
});

/**
 * startPicker, hoverElement and pickElement are slightly modified copies of
 * inspector's head.js helpers, but using spawnViewportTask to interact with the
 * content page (as well as some other slight modifications).
 */

async function startPicker(toolbox, ui) {
  info("Start the element picker");
  toolbox.win.focus();
  await toolbox.nodePicker.start();
  // By default make sure the content window is focused since the picker may not focus
  // the content window by default.
  await spawnViewportTask(ui, {}, async () => {
    content.focus();
  });
}

async function hoverElement(inspector, ui, selector, x, y) {
  info("Waiting for element " + selector + " to be hovered");
  const onHovered = inspector.toolbox.nodePicker.once("picker-node-hovered");
  await spawnViewportTask(ui, { selector, x, y }, async options => {
    const target = content.document.querySelector(options.selector);
    await EventUtils.synthesizeMouse(
      target,
      options.x,
      options.y,
      { type: "mousemove", isSynthesized: false },
      content
    );
  });
  return onHovered;
}

async function pickElement(inspector, ui, selector) {
  info("Waiting for element " + selector + " to be picked");
  const onNewNodeFront = inspector.selection.once("new-node-front");
  await spawnViewportTask(ui, { selector }, async options => {
    const target = content.document.querySelector(options.selector);
    EventUtils.synthesizeClick(target);
  });
  info("Returning on new-node-front");
  return onNewNodeFront;
}
