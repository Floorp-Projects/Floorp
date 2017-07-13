/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the Filter Editor Widget inputs increase/decrease value when cursor is
// on a number using arrow keys if cursor is behind/mid/after the number strings

const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");
const {getClientCssProperties} = require("devtools/shared/fronts/css-properties");

const DEFAULT_VALUE_MULTIPLIER = 1;

const TEST_URI = CHROME_URL_ROOT + "doc_filter-editor-01.html";

add_task(function* () {
  let [,, doc] = yield createHost("bottom", TEST_URI);
  const cssIsValid = getClientCssProperties().getValidityChecker(doc);

  const container = doc.querySelector("#filter-container");
  const initialValue = "drop-shadow(rgb(0, 0, 0) 10px 1px 0px)";
  let widget = new CSSFilterEditorWidget(container, initialValue, cssIsValid);
  const input = widget.el.querySelector("#filters input");

  let value = 10;

  triggerKey = triggerKey.bind(widget);

  info("Test increment/decrement of string-type numbers without selection");

  input.setSelectionRange(14, 14);
  triggerKey(40);

  value -= DEFAULT_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), val(value),
     "Should work with cursor in the middle of number");

  input.setSelectionRange(13, 13);
  triggerKey(38);

  value += DEFAULT_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), val(value),
     "Should work with cursor before the number");

  input.setSelectionRange(15, 15);
  triggerKey(40);

  value -= DEFAULT_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), val(value),
     "Should work with cursor after the number");

  info("Test increment/decrement of string-type numbers with a selection");

  input.setSelectionRange(13, 15);
  triggerKey(38);
  input.setSelectionRange(13, 18);
  triggerKey(38);

  value += DEFAULT_VALUE_MULTIPLIER * 2;
  is(widget.getValueAt(0), val(value),
     "Should work if a there is a selection, starting with the number");

  triggerKey = null;
});

// Triggers the specified keyCode and modifier key on
// first filter's input
function triggerKey(key, modifier) {
  const filter = this.el.querySelector("#filters").children[0];
  const input = filter.querySelector("input");

  this._keyDown({
    target: input,
    keyCode: key,
    [modifier]: true,
    preventDefault: function () {}
  });
}

function val(value) {
  let v = value.toFixed(1);

  if (v.indexOf(".0") > -1) {
    v = v.slice(0, -2);
  }
  return `rgb(0, 0, 0) ${v}px 1px 0px`;
}
