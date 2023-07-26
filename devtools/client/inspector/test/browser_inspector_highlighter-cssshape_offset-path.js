/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the CSS shapes highlighter for offset-path.

const TEST_URL = `data:text/html,<meta charset=utf8>${encodeURIComponent(`
  <style>
    html, body  {margin: 0; padding: 0; }

    .wrapper {
      width: 100px;
      height: 100px;
      background: tomato;
    }

    .target {
      width: 20px;
      height: 20px;
      display: block;
      background: gold;
      offset-path: inset(10% 20%);
    }

    #abs {
      position: absolute;
      background: cyan;
    }
  </style>
  <div class=wrapper>
    <span id=regular class=target></span>
    <span id=abs class=target></span>
  </div>`)}`;

const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(async function () {
  const env = await openInspectorForURL(TEST_URL);
  const { highlighterTestFront, inspector } = env;
  const view = selectRuleView(inspector);

  await selectNode("#regular", inspector);

  info(`Show offset-path shape highlighter for regular case`);
  await toggleShapesHighlighter(view, ".target", "offset-path", true);
  const rect = await getShapeHighlighterRect(highlighterTestFront, inspector);

  info(
    "Check that highlighter is drawn relatively to the selected node parent node"
  );

  const wrapperBoundingClientRect = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      const node = content.document.querySelector(".target");
      const { x, y, width, height } =
        node.parentElement.getBoundingClientRect();
      return { x, y, width, height };
    }
  );

  // Coming from inset(10% 20%)
  let inlineOffset = 0.2 * wrapperBoundingClientRect.width - 0.5;
  let blockOffset = 0.1 * wrapperBoundingClientRect.height - 0.5;

  is(rect.x, wrapperBoundingClientRect.x + inlineOffset, "Rect has expected x");
  is(rect.y, wrapperBoundingClientRect.y + blockOffset, "Rect has expected y");
  is(
    rect.width,
    wrapperBoundingClientRect.width - inlineOffset * 2,
    "Rect has expected width"
  );
  is(
    rect.height,
    wrapperBoundingClientRect.height - blockOffset * 2,
    "Rect has expected height"
  );

  info(`Hide offset-path shape highlighter`);
  await toggleShapesHighlighter(view, ".target", "offset-path", false);

  info(`Show offset-path shape highlighter for absolutely positioned element`);
  await selectNode("#abs", inspector);
  await toggleShapesHighlighter(view, ".target", "offset-path", true);

  const viewportClientRect = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      const [quad] = content.document.getBoxQuads();
      return {
        x: quad.p1.x,
        y: quad.p1.y,
        width: quad.p2.x - quad.p1.x,
        height: quad.p3.y - quad.p2.y,
      };
    }
  );

  const absRect = await getShapeHighlighterRect(
    highlighterTestFront,
    inspector
  );

  inlineOffset = 0.2 * viewportClientRect.width - 0.5;
  blockOffset = 0.1 * viewportClientRect.height - 0.5;

  is(
    Math.round(absRect.x),
    Math.round(viewportClientRect.x + inlineOffset),
    "Rect has expected x"
  );
  is(
    Math.round(absRect.y),
    Math.round(viewportClientRect.y + blockOffset),
    "Rect has expected y"
  );
  is(
    Math.round(absRect.width),
    Math.round(viewportClientRect.width - inlineOffset * 2),
    "Rect has expected width"
  );
  is(
    Math.round(absRect.height),
    Math.round(viewportClientRect.height - blockOffset * 2),
    "Rect has expected height"
  );

  info(`Hide offset-path shape highlighter for absolutely positioned element`);
  await toggleShapesHighlighter(view, ".target", "offset-path", false);
});

async function getShapeHighlighterRect(highlighterTestFront, inspector) {
  const highlighterFront =
    inspector.inspectorFront.getKnownHighlighter(HIGHLIGHTER_TYPE);
  return highlighterTestFront.getHighlighterBoundingClientRect(
    "shapes-rect",
    highlighterFront.actorID
  );
}
