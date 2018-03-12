/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the Filter Editor Widget inputs increase/decrease value when cursor is
// on a number using arrow keys, applying multiplier using alt/shift on strings

const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");
const {getClientCssProperties} = require("devtools/shared/fronts/css-properties");

const FAST_VALUE_MULTIPLIER = 10;
const SLOW_VALUE_MULTIPLIER = 0.1;
const DEFAULT_VALUE_MULTIPLIER = 1;

const TEST_URI = CHROME_URL_ROOT + "doc_filter-editor-01.html";

add_task(function* () {
  let [,, doc] = yield createHost("bottom", TEST_URI);
  const cssIsValid = getClientCssProperties().getValidityChecker(doc);

  const container = doc.querySelector("#filter-container");
  const initialValue = "drop-shadow(rgb(0, 0, 0) 1px 1px 0px)";
  let widget = new CSSFilterEditorWidget(container, initialValue, cssIsValid);
  widget.el.querySelector("#filters input").setSelectionRange(13, 13);

  let value = 1;

  triggerKey = triggerKey.bind(widget);

  info("Test simple arrow keys");
  triggerKey(40);

  value -= DEFAULT_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), val(value),
     "Should decrease value using down arrow");

  triggerKey(38);

  value += DEFAULT_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), val(value),
     "Should decrease value using down arrow");

  info("Test shift key multiplier");
  triggerKey(38, "shiftKey");

  value += FAST_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), val(value),
     "Should increase value by fast multiplier using up arrow");

  triggerKey(40, "shiftKey");

  value -= FAST_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), val(value),
     "Should decrease value by fast multiplier using down arrow");

  info("Test alt key multiplier");
  triggerKey(38, "altKey");

  value += SLOW_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), val(value),
     "Should increase value by slow multiplier using up arrow");

  triggerKey(40, "altKey");

  value -= SLOW_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), val(value),
     "Should decrease value by slow multiplier using down arrow");

  triggerKey(40, "shiftKey");

  value -= FAST_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), val(value),
     "Should decrease to negative");

  triggerKey(40);

  value -= DEFAULT_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), val(value),
     "Should decrease negative numbers correctly");

  triggerKey(38);

  value += DEFAULT_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), val(value),
     "Should increase negative values correctly");

  triggerKey(40, "altKey");
  triggerKey(40, "altKey");

  value -= SLOW_VALUE_MULTIPLIER * 2;
  is(widget.getValueAt(0), val(value),
     "Should decrease float numbers correctly");

  triggerKey(38, "altKey");

  value += SLOW_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), val(value),
     "Should increase float numbers correctly");

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
