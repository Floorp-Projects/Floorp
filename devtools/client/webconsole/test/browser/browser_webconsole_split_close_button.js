/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,<p>Web Console test for close button of " +
  "split console";

add_task(async function() {
  const toolbox = await openNewTabAndToolbox(TEST_URI, "inspector");

  info("Check the split console toolbar has a close button.");

  const onSplitConsoleReady = toolbox.once("webconsole-ready");
  toolbox.toggleSplitConsole();
  await onSplitConsoleReady;

  let closeButton = getCloseButton(toolbox);
  ok(closeButton, "The split console has close button.");

  info(
    "Check we can reopen split console after closing split console by using " +
      "the close button"
  );

  let onSplitConsoleChange = toolbox.once("split-console");
  closeButton.click();
  await onSplitConsoleChange;
  ok(!toolbox.splitConsole, "The split console has been closed.");

  onSplitConsoleChange = toolbox.once("split-console");
  toolbox.toggleSplitConsole();
  await onSplitConsoleChange;

  ok(toolbox.splitConsole, "The split console has been displayed.");
  closeButton = getCloseButton(toolbox);
  ok(closeButton, "The split console has the close button after reopening.");

  info("Check the close button is not displayed on console panel.");

  await toolbox.selectTool("webconsole");
  closeButton = getCloseButton(toolbox);
  ok(!closeButton, "The console panel should not have the close button.");

  info("The split console has the close button if back to the inspector.");

  await toolbox.selectTool("inspector");
  ok(
    toolbox.splitConsole,
    "The split console has been displayed with inspector."
  );
  closeButton = getCloseButton(toolbox);
  ok(closeButton, "The split console on the inspector has the close button.");
});

function getCloseButton(toolbox) {
  const hud = toolbox.getPanel("webconsole").hud;
  const doc = hud.ui.outputNode.ownerDocument;
  return doc.getElementById("split-console-close-button");
}
