/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that the eyedropper can copy colors to the clipboard

const HIGHLIGHTER_TYPE = "EyeDropper";
const ID = "eye-dropper-";
const TEST_URI = "data:text/html;charset=utf-8,<style>html{background:red}</style>";

add_task(function* () {
  let helper = yield openInspectorForURL(TEST_URI)
               .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));
  helper.prefix = ID;

  let {show, synthesizeKey, finalize} = helper;

  info("Show the eyedropper with the copyOnSelect option");
  yield show("html", {copyOnSelect: true});

  info("Make sure to wait until the eyedropper is done taking a screenshot of the page");
  yield waitForElementAttributeSet("root", "drawn", helper);

  yield waitForClipboard(() => {
    info("Activate the eyedropper so the background color is copied");
    let generateKey = synthesizeKey({key: "VK_RETURN", options: {}});
    generateKey.next();
  }, "#FF0000");

  ok(true, "The clipboard contains the right value");

  yield waitForElementAttributeRemoved("root", "drawn", helper);
  yield waitForElementAttributeSet("root", "hidden", helper);
  ok(true, "The eyedropper is now hidden");

  finalize();
});

function* waitForElementAttributeSet(id, name, {getElementAttribute}) {
  yield poll(function* () {
    let value = yield getElementAttribute(id, name);
    return !!value;
  }, `Waiting for element ${id} to have attribute ${name} set`);
}

function* waitForElementAttributeRemoved(id, name, {getElementAttribute}) {
  yield poll(function* () {
    let value = yield getElementAttribute(id, name);
    return !value;
  }, `Waiting for element ${id} to have attribute ${name} removed`);
}

function* poll(check, desc) {
  info(desc);

  for (let i = 0; i < 10; i++) {
    if (yield check()) {
      return;
    }
    yield new Promise(resolve => setTimeout(resolve, 200));
  }

  throw new Error(`Timeout while: ${desc}`);
}
