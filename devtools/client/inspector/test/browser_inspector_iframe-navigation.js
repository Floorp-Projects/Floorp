/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the highlighter element picker still works through iframe
// navigations.

const TEST_URI =
  "data:text/html;charset=utf-8," +
  "<p>bug 699308 - test iframe navigation</p>" +
  "<iframe src='data:text/html;charset=utf-8,hello world'></iframe>";

add_task(async function () {
  const { inspector, toolbox, highlighterTestFront } =
    await openInspectorForURL(TEST_URI);

  info("Starting element picker.");
  await startPicker(toolbox);

  info("Mouse over for body element.");
  await hoverElement(inspector, "body");

  let isVisible = await highlighterTestFront.isHighlighting();
  ok(isVisible, "Inspector is highlighting.");

  await reloadIframe(inspector);
  info("Frame reloaded. Reloading again.");

  await reloadIframe(inspector);
  info("Frame reloaded twice.");

  isVisible = await highlighterTestFront.isHighlighting();
  ok(isVisible, "Inspector is highlighting after iframe nav.");

  info("Stopping element picker.");
  await toolbox.nodePicker.stop({ canceled: true });
});

async function reloadIframe(inspector) {
  const { resourceCommand } = inspector.commands;

  const { onResource: onNewRoot } = await resourceCommand.waitForNextResource(
    resourceCommand.TYPES.ROOT_NODE,
    {
      ignoreExistingResources: true,
    }
  );

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    const iframeEl = content.document.querySelector("iframe");
    await new Promise(resolve => {
      iframeEl.addEventListener("load", () => resolve(), { once: true });
      iframeEl.contentWindow.location.reload();
    });
  });

  await onNewRoot;
}
