/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Tests the Filter Editor Widget's label-dragging

const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");
const {getClientCssProperties} = require("devtools/shared/fronts/css-properties");

const FAST_VALUE_MULTIPLIER = 10;
const SLOW_VALUE_MULTIPLIER = 0.1;
const DEFAULT_VALUE_MULTIPLIER = 1;

const GRAYSCALE_MAX = 100,
  GRAYSCALE_MIN = 0;

const TEST_URI = `data:text/html,<div id="filter-container" />`;

add_task(function* () {
  let [,, doc] = yield createHost("bottom", TEST_URI);
  const cssIsValid = getClientCssProperties().getValidityChecker(doc);

  const container = doc.querySelector("#filter-container");
  let widget = new CSSFilterEditorWidget(
    container, "grayscale(0%) url(test.svg)", cssIsValid
  );

  const filters = widget.el.querySelector("#filters");
  const grayscale = filters.children[0];
  const url = filters.children[1];

  info("Test label-dragging on number-type filters without modifiers");
  widget._mouseDown({
    target: grayscale.querySelector("label"),
    pageX: 0,
    altKey: false,
    shiftKey: false
  });

  widget._mouseMove({
    pageX: 12,
    altKey: false,
    shiftKey: false
  });
  let expected = DEFAULT_VALUE_MULTIPLIER * 12;
  is(widget.getValueAt(0),
     `${expected}%`,
     "Should update value correctly without modifiers");

  info("Test label-dragging on number-type filters with alt");
  widget._mouseMove({
    // 20 - 12 = 8
    pageX: 20,
    altKey: true,
    shiftKey: false
  });

  expected = expected + SLOW_VALUE_MULTIPLIER * 8;
  is(widget.getValueAt(0),
     `${expected}%`,
     "Should update value correctly with alt key");

  info("Test label-dragging on number-type filters with shift");
  widget._mouseMove({
    // 25 - 20 = 5
    pageX: 25,
    altKey: false,
    shiftKey: true
  });

  expected = expected + FAST_VALUE_MULTIPLIER * 5;
  is(widget.getValueAt(0),
     `${expected}%`,
     "Should update value correctly with shift key");

  info("Test releasing mouse and dragging again");

  widget._mouseUp();

  widget._mouseDown({
    target: grayscale.querySelector("label"),
    pageX: 0,
    altKey: false,
    shiftKey: false
  });

  widget._mouseMove({
    pageX: 5,
    altKey: false,
    shiftKey: false
  });

  expected = expected + DEFAULT_VALUE_MULTIPLIER * 5;
  is(widget.getValueAt(0),
     `${expected}%`,
     "Should reset multiplier to default");

  info("Test value ranges");

  widget._mouseMove({
    // 30 - 25 = 5
    pageX: 30,
    altKey: false,
    shiftKey: true
  });

  expected = GRAYSCALE_MAX;
  is(widget.getValueAt(0),
     `${expected}%`,
     "Shouldn't allow values higher than max");

  widget._mouseMove({
    pageX: -11,
    altKey: false,
    shiftKey: true
  });

  expected = GRAYSCALE_MIN;
  is(widget.getValueAt(0),
     `${expected}%`,
     "Shouldn't allow values less than min");

  widget._mouseUp();

  info("Test label-dragging on string-type filters");
  widget._mouseDown({
    target: url.querySelector("label"),
    pageX: 0,
    altKey: false,
    shiftKey: false
  });

  ok(!widget.isDraggingLabel,
     "Label-dragging should not work for string-type filters");

  widget._mouseMove({
    pageX: -11,
    altKey: false,
    shiftKey: true
  });

  is(widget.getValueAt(1),
     "test.svg",
     "Label-dragging on string-type filters shouldn't affect their value");
});
