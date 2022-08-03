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

  await areHandlersHiddenByDefault(helper);
  await areHandlersHiddenOnAreaCreation(helper);
  await areHandlersCorrectlyShownAfterAreaCreation(helper);

  info("Hiding the highlighter");
  await finalize();
});

async function areHandlersHiddenByDefault({ isElementHidden, mouse }) {
  info("Checking that highlighter's handlers are hidden by default");

  await mouse.down(X, Y);

  for (const handler of Object.keys(HANDLER_MAP)) {
    const hidden = await isElementHidden(`${HANDLER_PREFIX}${handler}`);
    ok(hidden, `${handler} handler is hidden by default`);
  }
}

async function areHandlersHiddenOnAreaCreation({ isElementHidden, mouse }) {
  info("Checking that highlighter's handlers are hidden while area creation");

  await mouse.move(X + WIDTH, Y + HEIGHT);

  for (const handler of Object.keys(HANDLER_MAP)) {
    const hidden = await isElementHidden(`${HANDLER_PREFIX}${handler}`);
    ok(hidden, `${handler} handler is still hidden on area creation`);
  }
}

async function areHandlersCorrectlyShownAfterAreaCreation(helper) {
  info("Checking that highlighter's handlers are shown after area creation");

  const { isElementHidden, mouse } = helper;

  await mouse.up();

  for (const handler of Object.keys(HANDLER_MAP)) {
    const hidden = await isElementHidden(`${HANDLER_PREFIX}${handler}`);
    ok(!hidden, `${handler} handler is shown after area creation`);

    const { x: handlerX, y: handlerY } = await getHandlerCoords(
      helper,
      handler
    );
    const { x: expectedX, y: expectedY } = HANDLER_MAP[handler](WIDTH, HEIGHT);
    is(handlerX, expectedX, `x coordinate of ${handler} handler is correct`);
    is(handlerY, expectedY, `y coordinate of ${handler} handler is correct`);
  }
}

async function getHandlerCoords({ getElementAttribute }, handler) {
  const handlerId = `${HANDLER_PREFIX}${handler}`;
  return {
    x: Math.round(await getElementAttribute(handlerId, "cx")),
    y: Math.round(await getElementAttribute(handlerId, "cy")),
  };
}
