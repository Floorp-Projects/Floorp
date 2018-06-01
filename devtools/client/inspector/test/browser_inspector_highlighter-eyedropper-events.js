/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test the eyedropper mouse and keyboard handling.

const HIGHLIGHTER_TYPE = "EyeDropper";
const ID = "eye-dropper-";
const TEST_URI = `
<style>
  html{width:100%;height:100%;}
</style>
<body>eye-dropper test</body>`;

const MOVE_EVENTS_DATA = [
  {type: "mouse", x: 200, y: 100, expected: {x: 200, y: 100}},
  {type: "mouse", x: 100, y: 200, expected: {x: 100, y: 200}},
  {type: "keyboard", key: "VK_LEFT", expected: {x: 99, y: 200}},
  {type: "keyboard", key: "VK_LEFT", shift: true, expected: {x: 89, y: 200}},
  {type: "keyboard", key: "VK_RIGHT", expected: {x: 90, y: 200}},
  {type: "keyboard", key: "VK_RIGHT", shift: true, expected: {x: 100, y: 200}},
  {type: "keyboard", key: "VK_DOWN", expected: {x: 100, y: 201}},
  {type: "keyboard", key: "VK_DOWN", shift: true, expected: {x: 100, y: 211}},
  {type: "keyboard", key: "VK_UP", expected: {x: 100, y: 210}},
  {type: "keyboard", key: "VK_UP", shift: true, expected: {x: 100, y: 200}},
  // Mouse initialization for left and top snapping
  {type: "mouse", x: 7, y: 7, expected: {x: 7, y: 7}},
  // Left Snapping
  {type: "keyboard", key: "VK_LEFT", shift: true, expected: {x: 0, y: 7},
   desc: "Left Snapping to x=0"},
  // Top Snapping
  {type: "keyboard", key: "VK_UP", shift: true, expected: {x: 0, y: 0},
   desc: "Top Snapping to y=0"},
  // Mouse initialization for right snapping
  {
    type: "mouse",
    x: (width, height) => width - 5,
    y: 0,
    expected: {
      x: (width, height) => width - 5,
      y: 0
    }
  },
  // Right snapping
  {
    type: "keyboard",
    key: "VK_RIGHT",
    shift: true,
    expected: {
      x: (width, height) => width,
      y: 0
    },
    desc: "Right snapping to x=max window width available"
  },
  // Mouse initialization for bottom snapping
  {
    type: "mouse",
    x: 0,
    y: (width, height) => height - 5,
    expected: {
      x: 0,
      y: (width, height) => height - 5
    }
  },
  // Bottom snapping
  {
    type: "keyboard",
    key: "VK_DOWN",
    shift: true,
    expected: {
      x: 0,
      y: (width, height) => height
    },
    desc: "Bottom snapping to y=max window height available"
  },
];

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const helper = await getHighlighterHelperFor(HIGHLIGHTER_TYPE)({inspector, testActor});

  helper.prefix = ID;

  await helper.show("html");
  await respondsToMoveEvents(helper, testActor);
  await respondsToReturnAndEscape(helper);

  helper.finalize();
});

async function respondsToMoveEvents(helper, testActor) {
  info("Checking that the eyedropper responds to events from the mouse and keyboard");
  const {mouse} = helper;
  const {width, height} = await testActor.getBoundingClientRect("html");

  for (let {type, x, y, key, shift, expected, desc} of MOVE_EVENTS_DATA) {
    x = typeof x === "function" ? x(width, height) : x;
    y = typeof y === "function" ? y(width, height) : y;
    expected.x = typeof expected.x === "function" ?
      expected.x(width, height) : expected.x;
    expected.y = typeof expected.y === "function" ?
      expected.y(width, height) : expected.y;

    if (typeof desc === "undefined") {
      info(`Simulating a ${type} event to move to ${expected.x} ${expected.y}`);
    } else {
      info(`Simulating ${type} event: ${desc}`);
    }

    if (type === "mouse") {
      await mouse.move(x, y);
    } else if (type === "keyboard") {
      const options = shift ? {shiftKey: true} : {};
      await EventUtils.synthesizeAndWaitKey(key, options);
    }
    await checkPosition(expected, helper);
  }
}

async function checkPosition({x, y}, {getElementAttribute}) {
  const style = await getElementAttribute("root", "style");
  is(style, `top:${y}px;left:${x}px;`,
     `The eyedropper is at the expected ${x} ${y} position`);
}

async function respondsToReturnAndEscape({isElementHidden, show}) {
  info("Simulating return to select the color and hide the eyedropper");

  await EventUtils.synthesizeAndWaitKey("VK_RETURN", {});
  let hidden = await isElementHidden("root");
  ok(hidden, "The eyedropper has been hidden");

  info("Showing the eyedropper again and simulating escape to hide it");

  await show("html");
  await EventUtils.synthesizeAndWaitKey("VK_ESCAPE", {});
  hidden = await isElementHidden("root");
  ok(hidden, "The eyedropper has been hidden again");
}
