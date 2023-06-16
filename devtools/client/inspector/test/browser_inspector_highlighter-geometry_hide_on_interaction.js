/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the geometry highlighter gets hidden when the user performs other actions.

const TEST_URL = `data:text/html;charset=utf-8,
   <h1 style='background:yellow;position:absolute;left:5rem;'>Hello</h1>`;

add_task(async function () {
  const { inspector, toolbox } = await openInspectorForURL(TEST_URL);

  info("Select the absolute positioned element");
  await selectNode("h1", inspector);
  const boxModelPanel = inspector.getPanel("boxmodel");

  info("Click on the button enabling the highlighter");
  const button = await waitFor(() =>
    boxModelPanel.document.querySelector(".layout-geometry-editor")
  );

  let onHighlighterShown = getOnceHighlighterShown(inspector);
  button.click();

  await onHighlighterShown;
  ok(true, "Highlighter is displayed");

  info("Check that hovering a node in the markup view hides the highlighter");
  const h1Nodecontainer = await getContainerForSelector("h1", inspector);

  let onHighlighterHidden = getOnceHighlighterHidden(inspector);
  EventUtils.synthesizeMouseAtCenter(
    h1Nodecontainer.tagLine,
    { type: "mousemove" },
    inspector.markup.doc.defaultView
  );
  await onHighlighterHidden;
  ok("Highlighter was hidden when hovering a node in the markup view");

  info("Check that leaving the markupview shows the highlighter again");
  onHighlighterShown = getOnceHighlighterShown(inspector);
  EventUtils.synthesizeMouseAtCenter(
    button,
    { type: "mousemove" },
    button.ownerDocument.defaultView
  );
  await onHighlighterShown;
  ok(true, "Highlighter is shown again when leaving the markup view");

  info("Check that starting the node picker disable the highlighter");
  onHighlighterHidden = getOnceHighlighterHidden(inspector);
  await startPicker(toolbox);
  await onHighlighterHidden;
  ok("Highlighter was hidden when using the node picker");

  // stop the node picker
  await toolbox.nodePicker.stop({ canceled: true });

  info("Check that selecting another node does hide the highlighter");
  onHighlighterShown = onHighlighterShown = getOnceHighlighterShown(inspector);
  button.click();
  await onHighlighterShown;
  ok(true, "highlighter is displayed again");

  onHighlighterHidden = onHighlighterHidden =
    getOnceHighlighterHidden(inspector);
  await selectNode("body", inspector);
  await onHighlighterHidden;
  ok(true, "Selecting another node hides the highlighter");
});

function getOnceHighlighterShown(inspector) {
  return inspector.highlighters.once("geometry-editor-highlighter-shown");
}

function getOnceHighlighterHidden(inspector) {
  return inspector.highlighters.once("geometry-editor-highlighter-hidden");
}
