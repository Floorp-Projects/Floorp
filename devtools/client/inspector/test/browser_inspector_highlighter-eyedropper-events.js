/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test the eyedropper mouse and keyboard handling.

const HIGHLIGHTER_TYPE = "EyeDropper";
const ID = "eye-dropper-";

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
];

add_task(function* () {
  let helper = yield openInspectorForURL("data:text/html;charset=utf-8,eye-dropper test")
               .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));
  helper.prefix = ID;

  yield helper.show("html");
  yield respondsToMoveEvents(helper);
  yield respondsToReturnAndEscape(helper);

  helper.finalize();
});

function* respondsToMoveEvents(helper) {
  info("Checking that the eyedropper responds to events from the mouse and keyboard");
  let {mouse} = helper;

  for (let {type, x, y, key, shift, expected} of MOVE_EVENTS_DATA) {
    info(`Simulating a ${type} event to move to ${expected.x} ${expected.y}`);
    if (type === "mouse") {
      yield mouse.move(x, y);
    } else if (type === "keyboard") {
      let options = shift ? {shiftKey: true} : {};
      yield EventUtils.synthesizeKey(key, options);
    }
    yield checkPosition(expected, helper);
  }
}

function* checkPosition({x, y}, {getElementAttribute}) {
  let style = yield getElementAttribute("root", "style");
  is(style, `top:${y}px;left:${x}px;`,
     `The eyedropper is at the expected ${x} ${y} position`);
}

function* respondsToReturnAndEscape({isElementHidden, show}) {
  info("Simulating return to select the color and hide the eyedropper");

  yield EventUtils.synthesizeKey("VK_RETURN", {});
  let hidden = yield isElementHidden("root");
  ok(hidden, "The eyedropper has been hidden");

  info("Showing the eyedropper again and simulating escape to hide it");

  yield show("html");
  yield EventUtils.synthesizeKey("VK_ESCAPE", {});
  hidden = yield isElementHidden("root");
  ok(hidden, "The eyedropper has been hidden again");
}
