/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the Filter Editor Widget inputs increase/decrease value using
// arrow keys, applying multiplier using alt/shift on number-type filters

const TEST_URI = "chrome://devtools/content/shared/widgets/filter-frame.xhtml";
const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");

const FAST_VALUE_MULTIPLIER = 10;
const SLOW_VALUE_MULTIPLIER = 0.1;
const DEFAULT_VALUE_MULTIPLIER = 1;

add_task(function*() {
  yield promiseTab("about:blank");
  let [host, win, doc] = yield createHost("bottom", TEST_URI);

  const container = doc.querySelector("#container");
  const initialValue = "blur(2px)";
  let widget = new CSSFilterEditorWidget(container, initialValue);

  let value = 2;

  triggerKey = triggerKey.bind(widget);

  info("Test simple arrow keys");
  triggerKey(40);

  value -= DEFAULT_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), `${value}px`,
     "Should decrease value using down arrow");

  triggerKey(38);

  value += DEFAULT_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), `${value}px`,
     "Should decrease value using down arrow");

  info("Test shift key multiplier");
  triggerKey(38, "shiftKey");

  value += FAST_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), `${value}px`,
     "Should increase value by fast multiplier using up arrow");

  triggerKey(40, "shiftKey");

  value -= FAST_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), `${value}px`,
     "Should decrease value by fast multiplier using down arrow");

  info("Test alt key multiplier");
  triggerKey(38, "altKey");

  value += SLOW_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), `${value}px`,
     "Should increase value by slow multiplier using up arrow");

  triggerKey(40, "altKey");

  value -= SLOW_VALUE_MULTIPLIER;
  is(widget.getValueAt(0), `${value}px`,
     "Should decrease value by slow multiplier using down arrow");

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
    preventDefault: function() {}
  });
}
