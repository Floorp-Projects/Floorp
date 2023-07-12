/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const IS_OSX = Services.appinfo.OS === "Darwin";
const VK_MOD = IS_OSX ? "VK_META" : "VK_CONTROL";
const TEST_URL = "data:text/html;charset=utf-8,measuring tool test";

const PREFIX = "measuring-tool-";
const HANDLER_PREFIX = "handler-";
const HIGHLIGHTED_HANDLER_CLASSNAME = "highlight";
const HIGHLIGHTER_TYPE = "MeasuringToolHighlighter";

const SMALL_DELTA = 1;
const LARGE_DELTA = 10;

add_task(async function () {
  const helper = await openInspectorForURL(TEST_URL).then(
    getHighlighterHelperFor(HIGHLIGHTER_TYPE)
  );

  const { show, finalize } = helper;

  helper.prefix = PREFIX;

  info("Showing the highlighter");
  await show();

  info("Creating the area");
  const { mouse } = helper;
  await mouse.down(32, 20);
  await mouse.move(32 + 160, 20 + 100);
  await mouse.up();

  const arrow_tests = [
    {
      key: "VK_LEFT",
      shift: false,
      delta: -SMALL_DELTA,
      move: "dx",
      resize: "dw",
    },
    {
      key: "VK_LEFT",
      shift: true,
      delta: -LARGE_DELTA,
      move: "dx",
      resize: "dw",
    },
    {
      key: "VK_RIGHT",
      shift: false,
      delta: SMALL_DELTA,
      move: "dx",
      resize: "dw",
    },
    {
      key: "VK_RIGHT",
      shift: true,
      delta: LARGE_DELTA,
      move: "dx",
      resize: "dw",
    },
    {
      key: "VK_UP",
      shift: false,
      delta: -SMALL_DELTA,
      move: "dy",
      resize: "dh",
    },
    {
      key: "VK_UP",
      shift: true,
      delta: -LARGE_DELTA,
      move: "dy",
      resize: "dh",
    },
    {
      key: "VK_DOWN",
      shift: false,
      delta: SMALL_DELTA,
      move: "dy",
      resize: "dh",
    },
    {
      key: "VK_DOWN",
      shift: true,
      delta: LARGE_DELTA,
      move: "dy",
      resize: "dh",
    },
  ];

  for (const { key, shift, delta, move, resize } of arrow_tests) {
    await canMoveAreaViaKeybindings(helper, key, shift, { [move]: delta });
    await canResizeAreaViaKeybindings(helper, key, shift, { [resize]: delta });
  }

  // Test handler highlighting on Ctrl/Command hold
  await handlerShouldNotBeHighlighted(helper);
  BrowserTestUtils.synthesizeKey(
    VK_MOD,
    { type: "keydown" },
    gBrowser.selectedBrowser
  );
  await handlerShouldBeHighlighted(helper);
  BrowserTestUtils.synthesizeKey("VK_LEFT", {}, gBrowser.selectedBrowser);
  await handlerShouldBeHighlighted(helper);
  BrowserTestUtils.synthesizeKey(
    VK_MOD,
    { type: "keyup" },
    gBrowser.selectedBrowser
  );
  await handlerShouldNotBeHighlighted(helper);

  info("Hiding the highlighter");
  await finalize();
});

async function canMoveAreaViaKeybindings(helper, key, shiftHeld, deltas) {
  const { dx = 0, dy = 0 } = deltas;

  const {
    x: origAreaX,
    y: origAreaY,
    width: origAreaWidth,
    height: origAreaHeight,
  } = await getAreaRect(helper);

  const eventOptions = shiftHeld ? { shiftKey: true } : {};
  BrowserTestUtils.synthesizeKey(key, eventOptions, gBrowser.selectedBrowser);

  const {
    x: areaX,
    y: areaY,
    width: areaWidth,
    height: areaHeight,
  } = await getAreaRect(helper);

  is(areaX, origAreaX + dx, "X coordinate correct after moving");
  is(areaY, origAreaY + dy, "Y coordinate correct after moving");
  is(areaWidth, origAreaWidth, "Width unchanged after moving");
  is(areaHeight, origAreaHeight, "Height unchanged after moving");
}

async function canResizeAreaViaKeybindings(helper, key, shiftHeld, deltas) {
  const { dw = 0, dh = 0 } = deltas;

  const {
    x: origAreaX,
    y: origAreaY,
    width: origAreaWidth,
    height: origAreaHeight,
  } = await getAreaRect(helper);

  const eventOptions = IS_OSX ? { metaKey: true } : { ctrlKey: true };
  if (shiftHeld) {
    eventOptions.shiftKey = true;
  }
  BrowserTestUtils.synthesizeKey(key, eventOptions, gBrowser.selectedBrowser);

  const {
    x: areaX,
    y: areaY,
    width: areaWidth,
    height: areaHeight,
  } = await getAreaRect(helper);

  is(areaX, origAreaX, "X coordinate unchanged after resizing");
  is(areaY, origAreaY, "Y coordinate unchanged after resizing");
  is(areaWidth, origAreaWidth + dw, "Width correct after resizing");
  is(areaHeight, origAreaHeight + dh, "Height correct after resizing");
}

async function handlerIsHighlighted(helper) {
  const klass = await helper.getElementAttribute(
    `${HANDLER_PREFIX}topleft`,
    "class"
  );
  return klass.includes(HIGHLIGHTED_HANDLER_CLASSNAME);
}

async function handlerShouldBeHighlighted(helper) {
  ok(await handlerIsHighlighted(helper), "Origin handler is highlighted");
}

async function handlerShouldNotBeHighlighted(helper) {
  ok(
    !(await handlerIsHighlighted(helper)),
    "Origin handler is not highlighted"
  );
}
