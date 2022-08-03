/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "data:text/html;charset=utf-8,measuring tool test";

const PREFIX = "measuring-tool-";
const HANDLER_PREFIX = "handler-";
const HIGHLIGHTER_TYPE = "MeasuringToolHighlighter";

const X = 32;
const Y = 20;
const WIDTH = 160;
const HEIGHT = 100;
const X_OFFSET = 15;
const Y_OFFSET = 10;

const HANDLER_MAP = {
  top(areaWidth, areaHeight) {
    return { x: Math.round(areaWidth / 2), y: 0 };
  },
  topright(areaWidth, areaHeight) {
    return { x: areaWidth, y: 0 };
  },
  right(areaWidth, areaHeight) {
    return { x: areaWidth, y: Math.round(areaHeight / 2) };
  },
  bottomright(areaWidth, areaHeight) {
    return { x: areaWidth, y: areaHeight };
  },
  bottom(areaWidth, areaHeight) {
    return { x: Math.round(areaWidth / 2), y: areaHeight };
  },
  bottomleft(areaWidth, areaHeight) {
    return { x: 0, y: areaHeight };
  },
  left(areaWidth, areaHeight) {
    return { x: 0, y: Math.round(areaHeight / 2) };
  },
  topleft(areaWidth, areaHeight) {
    return { x: 0, y: 0 };
  },
};

add_task(async function() {
  const helper = await openInspectorForURL(TEST_URL).then(
    getHighlighterHelperFor(HIGHLIGHTER_TYPE)
  );

  const { show, finalize } = helper;

  helper.prefix = PREFIX;

  info("Showing the highlighter");
  await show();

  info("Creating the area");
  const { mouse } = helper;
  await mouse.down(X, Y);
  await mouse.move(X + WIDTH, Y + HEIGHT);
  await mouse.up();

  await canResizeAreaViaHandlers(helper);

  info("Hiding the highlighter");
  await finalize();
});

async function canResizeAreaViaHandlers(helper) {
  const { mouse } = helper;

  for (const handler of Object.keys(HANDLER_MAP)) {
    const { x: origHandlerX, y: origHandlerY } = await getHandlerCoords(
      helper,
      handler
    );
    const {
      x: origAreaX,
      y: origAreaY,
      width: origAreaWidth,
      height: origAreaHeight,
    } = await getAreaCoords(helper);
    const absOrigHandlerX = origHandlerX + origAreaX;
    const absOrigHandlerY = origHandlerY + origAreaY;

    const delta = {
      x: handler.includes("left") ? X_OFFSET : 0,
      y: handler.includes("top") ? Y_OFFSET : 0,
      width:
        handler.includes("right") || handler.includes("left") ? X_OFFSET : 0,
      height:
        handler.includes("bottom") || handler.includes("top") ? Y_OFFSET : 0,
    };

    if (handler.includes("left")) {
      delta.width *= -1;
    }
    if (handler.includes("top")) {
      delta.height *= -1;
    }

    // Simulate drag & drop of handler
    await mouse.down(absOrigHandlerX, absOrigHandlerY);
    await mouse.move(absOrigHandlerX + X_OFFSET, absOrigHandlerY + Y_OFFSET);
    await mouse.up();

    const {
      x: areaX,
      y: areaY,
      width: areaWidth,
      height: areaHeight,
    } = await getAreaCoords(helper);
    is(
      areaX,
      origAreaX + delta.x,
      `X coordinate of area correct after resizing using ${handler} handler`
    );
    is(
      areaY,
      origAreaY + delta.y,
      `Y coordinate of area correct after resizing using ${handler} handler`
    );
    is(
      areaWidth,
      origAreaWidth + delta.width,
      `Width of area correct after resizing using ${handler} handler`
    );
    is(
      areaHeight,
      origAreaHeight + delta.height,
      `Height of area correct after resizing using ${handler} handler`
    );

    const { x: handlerX, y: handlerY } = await getHandlerCoords(
      helper,
      handler
    );
    const { x: expectedX, y: expectedY } = HANDLER_MAP[handler](
      areaWidth,
      areaHeight
    );
    is(
      handlerX,
      expectedX,
      `X coordinate of ${handler} handler correct after resizing`
    );
    is(
      handlerY,
      expectedY,
      `Y coordinate of ${handler} handler correct after resizing`
    );
  }
}

async function getAreaCoords({ getElementAttribute }) {
  // The 'box-path' element holds the width and height of the
  // measuring area as well as the position relative to its
  // parent <g> element.
  const d = await getElementAttribute("box-path", "d");
  // The tool element itself is a <g> element grouping all paths.
  // Though <g> elements do not have coordinates by themselves,
  // therefore it is positioned using the 'transform' CSS property.
  // So, in order to get the position of the measuring area, the
  // coordinates need to be read from the translate() function.
  const transform = await getElementAttribute("tool", "transform");
  const reDir = /(\d+) (\d+)/g;
  const reTransform = /(\d+),(\d+)/;
  const coords = {
    x: 0,
    y: 0,
    width: 0,
    height: 0,
  };
  let match;
  while ((match = reDir.exec(d))) {
    let [, x, y] = match;
    x = Number(x);
    y = Number(y);
    if (x < coords.x) {
      coords.x = x;
    }
    if (y < coords.y) {
      coords.y = y;
    }
    if (x > coords.width) {
      coords.width = x;
    }
    if (y > coords.height) {
      coords.height = y;
    }
  }

  match = reTransform.exec(transform);
  coords.x += Number(match[1]);
  coords.y += Number(match[2]);

  return coords;
}

async function getHandlerCoords({ getElementAttribute }, handler) {
  const handlerId = `${HANDLER_PREFIX}${handler}`;
  return {
    x: Math.round(await getElementAttribute(handlerId, "cx")),
    y: Math.round(await getElementAttribute(handlerId, "cy")),
  };
}
