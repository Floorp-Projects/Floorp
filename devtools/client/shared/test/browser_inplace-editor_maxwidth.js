/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { editableField } = require("devtools/client/shared/inplace-editor");

const MAX_WIDTH = 300;
const START_TEXT = "Start text";
const LONG_TEXT = "I am a long text and I will not fit in a 300px container. " +
  "I expect the inplace editor to wrap.";

// Test the inplace-editor behavior with a maxWidth configuration option
// defined.

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8,inplace editor max width tests");
  let [host, , doc] = yield createHost();

  info("Testing the maxWidth option in pixels, to precisely check the size");
  yield new Promise(resolve => {
    createInplaceEditorAndClick({
      multiline: true,
      maxWidth: MAX_WIDTH,
      start: testMaxWidth,
      done: resolve
    }, doc);
  });

  host.destroy();
  gBrowser.removeCurrentTab();
});

let testMaxWidth = Task.async(function* (editor) {
  is(editor.input.value, START_TEXT, "Span text content should be used");
  ok(editor.input.offsetWidth < MAX_WIDTH,
    "Input width should be strictly smaller than MAX_WIDTH");
  is(getLines(editor.input), 1, "Input should display 1 line of text");

  info("Check a text is on several lines if it does not fit MAX_WIDTH");
  for (let key of LONG_TEXT) {
    EventUtils.sendChar(key);
    checkScrollbars(editor.input);
  }

  is(editor.input.value, LONG_TEXT, "Long text should be the input value");
  is(editor.input.offsetWidth, MAX_WIDTH,
    "Input width should be the same as MAX_WIDTH");
  is(getLines(editor.input), 3, "Input should display 3 lines of text");
  checkScrollbars(editor.input);

  info("Delete all characters on line 3.");
  while (getLines(editor.input) === 3) {
    EventUtils.sendKey("BACK_SPACE");
    checkScrollbars(editor.input);
  }

  is(editor.input.offsetWidth, MAX_WIDTH,
    "Input width should be the same as MAX_WIDTH");
  is(getLines(editor.input), 2, "Input should display 2 lines of text");
  checkScrollbars(editor.input);

  info("Delete all characters on line 2.");
  while (getLines(editor.input) === 2) {
    EventUtils.sendKey("BACK_SPACE");
    checkScrollbars(editor.input);
  }

  is(getLines(editor.input), 1, "Input should display 1 line of text");
  checkScrollbars(editor.input);

  info("Delete all characters.");
  while (editor.input.value !== "") {
    EventUtils.sendKey("BACK_SPACE");
    checkScrollbars(editor.input);
  }

  ok(editor.input.offsetWidth < MAX_WIDTH,
    "Input width should again be strictly smaller than MAX_WIDTH");
  ok(editor.input.offsetWidth > 0,
    "Even with no content, the input has a non-zero width");
  is(getLines(editor.input), 1, "Input should display 1 line of text");
  checkScrollbars(editor.input);

  info("Leave the inplace-editor");
  EventUtils.sendKey("RETURN");
});

/**
 * Retrieve the current number of lines displayed in the provided textarea.
 *
 * @param {DOMNode} textarea
 * @return {Number} the number of lines
 */
function getLines(textarea) {
  let win = textarea.ownerDocument.defaultView;
  let style = win.getComputedStyle(textarea);
  let lineHeight = style.getPropertyCSSValue("line-height").cssText;
  return Math.floor(textarea.clientHeight / parseFloat(lineHeight));
}

/**
 * Verify that the provided textarea has no vertical or horizontal scrollbar.
 *
 * @param {DOMNode} textarea
 */
function checkScrollbars(textarea) {
  is(textarea.scrollHeight, textarea.clientHeight,
    "Textarea should never have vertical scrollbars");
  is(textarea.scrollWidth, textarea.clientWidth,
    "Textarea should never have horizontal scrollbars");
}

function createInplaceEditorAndClick(options, doc) {
  doc.body.innerHTML = "";
  let span = options.element = createSpan(doc);

  info("Creating an inplace-editor field");
  editableField(options);

  info("Clicking on the inplace-editor field to turn to edit mode");
  span.click();
}

function createSpan(doc) {
  info("Creating a new span element");
  let span = doc.createElement("span");
  span.setAttribute("tabindex", "0");
  span.style.fontSize = "11px";
  span.style.fontFamily = "monospace";
  span.textContent = START_TEXT;
  doc.body.appendChild(span);
  return span;
}
