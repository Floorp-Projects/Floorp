/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the shapes highlighter can be toggled for basic shapes with default values.

const TEST_URI = `
  <style type='text/css'>
    #shape-circle {
      clip-path: circle();
    }
    #shape-ellipse {
      clip-path: ellipse();
    }
    #shape-inset {
      clip-path: inset();
    }
    #shape-polygon {
      clip-path: polygon();
    }
  </style>
  <div id="shape-circle"></div>
  <div id="shape-ellipse"></div>
  <div id="shape-inset"></div>
  <div id="shape-polygon"></div>
`;

const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  const highlighters = view.highlighters;

  const selectors = new Map([
    ["#shape-circle", true],
    ["#shape-ellipse", true],
    // Basic shapes inset() and polygon() expect explicit coordinates.
    // They don't have default values and are invalid without coordinates.
    ["#shape-inset", false],
    ["#shape-polygon", false],
  ]);

  for (const [selector, expectShapesToogle] of selectors) {
    await selectNode(selector, inspector);
    const container = getRuleViewProperty(view, selector, "clip-path")
      .valueSpan;
    const shapesToggle = container.querySelector(".ruleview-shapeswatch");

    if (expectShapesToogle) {
      ok(
        shapesToggle,
        `Shapes highlighter toggle expected and found for ${selector}`
      );
    } else {
      is(
        shapesToggle,
        null,
        `Shapes highlighter toggle not expected and not found for ${selector}`
      );

      // Skip the rest of the test.
      continue;
    }

    info(`Toggling ON the shapes highlighter for ${selector}`);
    const onHighlighterShown = highlighters.once("shapes-highlighter-shown");
    shapesToggle.click();
    await onHighlighterShown;

    ok(
      shapesToggle.classList.contains("active"),
      `Shapes highlighter toggle active for ${selector}`
    );
    ok(
      highlighters.highlighters[HIGHLIGHTER_TYPE],
      `Shapes highlighter instance created for ${selector}`
    );
    ok(
      highlighters.shapesHighlighterShown,
      `Shapes highlighter shown for ${selector}`
    );

    info(`Toggling OFF the shapes highlighter for ${selector}`);
    const onHighlighterHidden = highlighters.once("shapes-highlighter-hidden");
    shapesToggle.click();
    await onHighlighterHidden;

    ok(
      !shapesToggle.classList.contains("active"),
      `Shapes highlighter toggle no longer active for ${selector}`
    );
    ok(
      !highlighters.shapesHighlighterShown,
      `Shapes highlighter no longer shown for ${selector}`
    );
  }
});
