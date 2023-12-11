/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the shapes highlighter in the rule view and the display of the
// shapes highlighter.

const TEST_URI = `
  <style type='text/css'>
    html, body {
      margin: 0;
      padding: 0;
    }

    #shape {
      width: 800px;
      height: 800px;
      background: tomato;
      clip-path: circle(25%);
    }
  </style>
  <div id="shape"></div>
`;

const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view, highlighterTestFront } = await openRuleView();
  const highlighters = view.highlighters;

  info("Select a node with a shape value");
  await selectNode("#shape", inspector);
  const container = getRuleViewProperty(view, "#shape", "clip-path").valueSpan;
  const shapesToggle = container.querySelector(".ruleview-shapeswatch");

  info("Checking the initial state of the CSS shape toggle in the rule-view.");
  ok(shapesToggle, "Shapes highlighter toggle is visible.");
  ok(
    !shapesToggle.classList.contains("active"),
    "Shapes highlighter toggle button is not active."
  );
  ok(
    !highlighters.highlighters[HIGHLIGHTER_TYPE],
    "No CSS shapes highlighter exists in the rule-view."
  );
  ok(
    !highlighters.shapesHighlighterShown,
    "No CSS shapes highlighter is shown."
  );
  info("Toggling ON the CSS shapes highlighter from the rule-view.");
  const onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  shapesToggle.click();
  await onHighlighterShown;

  info(
    "Checking the CSS shapes highlighter is created and toggle button is active in " +
      "the rule-view."
  );
  ok(
    shapesToggle.classList.contains("active"),
    "Shapes highlighter toggle is active."
  );
  ok(
    inspector.inspectorFront.getKnownHighlighter(HIGHLIGHTER_TYPE).actorID,
    "CSS shapes highlighter created in the rule-view."
  );
  ok(highlighters.shapesHighlighterShown, "CSS shapes highlighter is shown.");

  const helper = await getHighlighterHelperFor(HIGHLIGHTER_TYPE)({
    inspector,
    highlighterTestFront,
  });

  info("Update shape");
  const { mouse } = helper;

  let onRuleViewChanged = view.once("ruleview-changed");
  // we want to change the radius. clip-path is 25%, element is 800px wide, meaning that
  // the center is at 400,400 and so the right handle is at (400 + 800px*0.25),400 -> 600,400
  await mouse.down(600, 400);
  // moving the handle 200px to the right to make the clip-path 50%
  await mouse.move(800, 400);
  await mouse.up();
  await reflowContentPage();
  await onRuleViewChanged;
  let newValue = getRuleViewPropertyValue(view, "#shape", "clip-path");
  is(
    newValue,
    `circle(50.00%)`,
    "clip-path property was updated when changing the radius"
  );

  onRuleViewChanged = view.once("ruleview-changed");
  // moving the circle, which center is 400,400 as the div is 800px wide and tall
  await mouse.down(400, 400);
  await mouse.move(200, 400);
  await mouse.up();
  await reflowContentPage();
  await onRuleViewChanged;
  newValue = getRuleViewPropertyValue(view, "#shape", "clip-path");
  is(
    newValue,
    `circle(50% at 200px 400px)`,
    "clip-path property was updated (position was added) when moving the shape"
  );

  onRuleViewChanged = view.once("ruleview-changed");
  // change the radius again to check that it handles the position
  await mouse.down(600, 400);
  await mouse.move(400, 400);
  await mouse.up();
  await reflowContentPage();
  await onRuleViewChanged;
  newValue = getRuleViewPropertyValue(view, "#shape", "clip-path");
  is(
    newValue,
    `circle(25.00% at 200px 400px)`,
    "clip-path property (with position) was updated when changing the radius"
  );

  info("Toggling OFF the CSS shapes highlighter from the rule-view.");
  const onHighlighterHidden = highlighters.once("shapes-highlighter-hidden");
  shapesToggle.click();
  await onHighlighterHidden;

  info(
    "Checking the CSS shapes highlighter is not shown and toggle button is not " +
      "active in the rule-view."
  );
  ok(
    !shapesToggle.classList.contains("active"),
    "shapes highlighter toggle button is not active."
  );
  ok(
    !highlighters.shapesHighlighterShown,
    "No CSS shapes highlighter is shown."
  );
});
