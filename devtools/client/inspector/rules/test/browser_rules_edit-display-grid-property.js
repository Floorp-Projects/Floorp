/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the grid highlighter in the rule view and modifying the 'display: grid'
// declaration.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid">
    <div id="cell1">cell1</div>
    <div id="cell2">cell2</div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  const highlighters = view.highlighters;
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.GRID;
  const {
    waitForHighlighterTypeShown,
    waitForHighlighterTypeHidden,
  } = getHighlighterTestHelpers(inspector);

  await selectNode("#grid", inspector);
  const container = (
    await getRuleViewProperty(view, "#grid", "display", { wait: true })
  ).valueSpan;
  let gridToggle = container.querySelector(".js-toggle-grid-highlighter");

  info("Toggling ON the CSS grid highlighter from the rule-view.");
  const onHighlighterShown = waitForHighlighterTypeShown(HIGHLIGHTER_TYPE);
  gridToggle.click();
  await onHighlighterShown;

  info("Edit the 'grid' property value to 'block'.");
  const editor = await focusEditableField(view, container);
  const onHighlighterHidden = waitForHighlighterTypeHidden(HIGHLIGHTER_TYPE);
  const onDone = view.once("ruleview-changed");
  editor.input.value = "block;";
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  await onHighlighterHidden;
  await onDone;

  info("Check the grid highlighter and grid toggle button are hidden.");
  gridToggle = container.querySelector(".js-toggle-grid-highlighter");
  ok(!gridToggle, "Grid highlighter toggle is not visible.");
  ok(!highlighters.gridHighlighters.size, "No CSS grid highlighter is shown.");
});
