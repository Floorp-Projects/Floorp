/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the selector highlighter toggling mechanism works correctly.

const TEST_URI = `
  <style type="text/css">
    div {text-decoration: underline;}
    .node-1 {color: red;}
    .node-2 {color: green;}
  </style>
  <div class="node-1">Node 1</div>
  <div class="node-2">Node 2</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  let data;

  info("Select .node-1 and click on the .node-1 selector icon");
  await selectNode(".node-1", inspector);
  data = await clickSelectorIcon(view, ".node-1");
  ok(data.isShown, "The highlighter is shown");

  info("With .node-1 still selected, click again on the .node-1 selector icon");
  data = await clickSelectorIcon(view, ".node-1");
  ok(!data.isShown, "The highlighter is now hidden");

  info("With .node-1 still selected, click on the div selector icon");
  data = await clickSelectorIcon(view, "div");
  ok(data.isShown, "The highlighter is shown again");

  info("With .node-1 still selected, click again on the .node-1 selector icon");
  data = await clickSelectorIcon(view, ".node-1");
  ok(
    data.isShown,
    "The highlighter is shown again since the clicked selector was different"
  );

  info("Selecting .node-2");
  await selectNode(".node-2", inspector);
  const activeHighlighter = inspector.highlighters.getActiveHighlighter(
    inspector.highlighters.TYPES.SELECTOR
  );
  ok(activeHighlighter, "The highlighter is still shown after selection");

  info("With .node-2 selected, click on the div selector icon");
  data = await clickSelectorIcon(view, "div");
  ok(
    data.isShown,
    "The highlighter is shown still since the selected was different"
  );

  info("Switching back to .node-1 and clicking on the div selector");
  await selectNode(".node-1", inspector);
  data = await clickSelectorIcon(view, "div");
  ok(
    !data.isShown,
    "The highlighter is hidden now that the same selector was clicked"
  );
});
