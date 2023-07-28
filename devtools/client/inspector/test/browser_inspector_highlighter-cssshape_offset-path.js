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
      offset-path: inset(10% 20%);
    }
  </style>
  <div class=wrapper>
    <span id=regular class=target></span>
    <span id=abs class=target></span>
  </div>`)}`;

const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(async function () {
  await pushPref("layout.css.motion-path-basic-shapes.enabled", true);
  const env = await openInspectorForURL(TEST_URL);
  const { highlighterTestFront, inspector } = env;
  const view = selectRuleView(inspector);

  await selectNode("#regular", inspector);

  info(`Show offset-path shape highlighter for regular case`);
  await toggleShapesHighlighter(view, ".target", "offset-path", true);

  info(
    "Check that highlighter is drawn relatively to the selected node parent node"
  );

  const wrapperQuads = await getAllAdjustedQuadsForContentPageElement(
    ".wrapper"
  );
  const {
    width: wrapperWidth,
    height: wrapperHeight,
    x: wrapperX,
    y: wrapperY,
  } = wrapperQuads.content[0].bounds;

  const rect = await getShapeHighlighterRect(highlighterTestFront, inspector);
  // SVG stroke seems to impact boundingClientRect differently depending on platform/hardware.
  // Let's assert that the delta is okay, and use it for the different assertions.
  const delta = wrapperX + wrapperWidth * 0.2 - rect.x;
  ok(Math.abs(delta) <= 1, `delta is <=1 (${Math.abs(delta)})`);

  // Coming from inset(10% 20%)
  let inlineOffset = 0.2 * wrapperWidth - delta;
  let blockOffset = 0.1 * wrapperHeight - delta;

  is(rect.x, wrapperX + inlineOffset, "Rect has expected x");
  is(rect.y, wrapperY + blockOffset, "Rect has expected y");
  is(rect.width, wrapperWidth - inlineOffset * 2, "Rect has expected width");
  is(rect.height, wrapperHeight - blockOffset * 2, "Rect has expected height");

  const x = parseFloat(
    await highlighterTestFront.getHighlighterNodeAttribute(
      "shapes-rect",
      "x",
      inspector.inspectorFront.getKnownHighlighter(HIGHLIGHTER_TYPE)
    )
  );
  const y = parseFloat(
    await highlighterTestFront.getHighlighterNodeAttribute(
      "shapes-rect",
      "y",
      inspector.inspectorFront.getKnownHighlighter(HIGHLIGHTER_TYPE)
    )
  );
  const width = parseFloat(
    await highlighterTestFront.getHighlighterNodeAttribute(
      "shapes-rect",
      "width",
      inspector.inspectorFront.getKnownHighlighter(HIGHLIGHTER_TYPE)
    )
  );

  const left = (wrapperWidth * x) / 100;
  const top = (wrapperHeight * y) / 100;
  const right = left + (wrapperWidth * width) / 100;
  const xCenter = (left + right) / 2;
  const dy = wrapperHeight / 10;

  const helper = await getHighlighterHelperFor(HIGHLIGHTER_TYPE)(env);
  const { mouse } = helper;

  info("Moving inset top");
  const onShapeChangeApplied = view.highlighters.once(
    "shapes-highlighter-changes-applied"
  );
  await mouse.down(xCenter, top, ".wrapper");
  await mouse.move(xCenter, top + dy, ".wrapper");
  await mouse.up(xCenter, top + dy, ".wrapper");
  await reflowContentPage();
  await onShapeChangeApplied;

  const offsetPathAfterUpdate = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () =>
      content.getComputedStyle(content.document.getElementById("regular"))
        .offsetPath
  );

  is(
    offsetPathAfterUpdate,
    `inset(${top + dy}% 20% 10%)`,
    "Inset edges successfully moved"
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

  inlineOffset = 0.2 * viewportClientRect.width - delta;
  blockOffset = 0.1 * viewportClientRect.height - delta;

  ok(
    Math.abs(absRect.x - (viewportClientRect.x + inlineOffset)) < 1,
    `Rect approximately has expected x (got ${absRect.x}, expected ${
      viewportClientRect.x + inlineOffset
    })`
  );
  ok(
    Math.abs(absRect.y - (viewportClientRect.y + blockOffset)) < 1,
    `Rect approximately has expected y (got ${absRect.y}, expected ${
      viewportClientRect.y + blockOffset
    })`
  );
  ok(
    Math.abs(absRect.width - (viewportClientRect.width - inlineOffset * 2)) < 1,
    `Rect approximately has expected width (got ${absRect.width}, expected ${
      viewportClientRect.width - inlineOffset * 2
    })`
  );
  ok(
    Math.abs(absRect.height - (viewportClientRect.height - blockOffset * 2)) <
      1,
    `Rect approximately has expected height (got ${absRect.height}, expected ${
      viewportClientRect.height - blockOffset * 2
    })`
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
