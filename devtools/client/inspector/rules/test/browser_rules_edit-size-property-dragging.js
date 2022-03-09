/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that increasing / decreasing values in rule view by dragging with
// the mouse works correctly.

const TEST_URI = `
  <style>
    #test {
      padding-top: 10px;
      margin-top: unset;
      margin-bottom: 0px;
      width: 0px;
      border: 1px solid red;
      line-height: 2;
      border-width: var(--12px);
      max-height: +10.2e3vmin;
      min-height: 1% !important;
      font-size: 10Q;
      transform: rotate(45deg);
      margin-left: 28.3em;
      animation-delay: +15s;
      margin-right: -2px;
      padding-bottom: .9px;
      rotate: 90deg;
    }
  </style>
  <div id="test"></div>
`;

const DRAGGABLE_VALUE_CLASSNAME = "ruleview-propertyvalue-draggable";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { inspector, view } = await openRuleView();
  await selectNode("#test", inspector);

  testDraggingClassIsAddedWhenNeeded(view);
  await testIncrementAngleValue(view);
  await testPressingEscapeWhileDragging(view);
  await testUpdateDisabledValue(view);
  await testWidthIncrements(view);
  await testDraggingClassIsAddedOnValueUpdate(view);
});

function testDraggingClassIsAddedWhenNeeded(view) {
  info("Testing class is added or not on different property values");
  const properties = [
    {
      name: "border",
      value: "1px solid red",
      shouldBeDraggable: false,
    },
    {
      name: "line-height",
      value: "2",
      shouldBeDraggable: false,
    },
    {
      name: "border-width",
      value: "var(--12px)",
      shouldBeDraggable: false,
    },
    {
      name: "transform",
      value: "rotate(45deg)",
      shouldBeDraggable: false,
    },
    {
      name: "max-height",
      value: "+10.2e3vmin",
      shouldBeDraggable: true,
    },
    {
      name: "min-height",
      value: "1%",
      shouldBeDraggable: true,
    },
    {
      name: "font-size",
      value: "10Q",
      shouldBeDraggable: true,
    },
    {
      name: "margin-left",
      value: "28.3em",
      shouldBeDraggable: true,
    },
    {
      name: "animation-delay",
      value: "+15s",
      shouldBeDraggable: true,
    },
    {
      name: "margin-right",
      value: "-2px",
      shouldBeDraggable: true,
    },
    {
      name: "padding-bottom",
      value: ".9px",
      shouldBeDraggable: true,
    },
    {
      name: "rotate",
      value: "90deg",
      shouldBeDraggable: true,
    },
  ];
  runIsDraggableTest(view, properties);
}

async function testIncrementAngleValue(view) {
  info("Testing updating an angle value with the angle swatch span");
  const rotatePropEditor = getTextProperty(view, 1, {
    rotate: "90deg",
  }).editor;
  await runIncrementTest(rotatePropEditor, view, [
    {
      startValue: "90deg",
      expectedEndValue: "100deg",
      distance: 10,
      description: "updating angle value",
    },
  ]);
}

async function testPressingEscapeWhileDragging(view) {
  info("Testing pressing escape while dragging with mouse");
  const marginPropEditor = getTextProperty(view, 1, { "margin-bottom": "0px" })
    .editor;
  await runIncrementTest(marginPropEditor, view, [
    {
      startValue: "0px",
      expectedEndValue: "0px",
      expectedEndValueBeforeEscape: "100px",
      escape: true,
      distance: 100,
      description: "Pressing escape to check if value has been reset",
    },
  ]);
}

async function testUpdateDisabledValue(view) {
  info("Testing updating a disabled value by dragging mouse");

  const textProperty = getTextProperty(view, 1, { "padding-top": "10px" });
  const editor = textProperty.editor;

  await togglePropStatus(view, textProperty);
  ok(!editor.enable.checked, "Should be disabled");
  await runIncrementTest(editor, view, [
    {
      startValue: "10px",
      expectedEndValue: "110px",
      distance: 100,
      description: "Updating disabled value",
    },
  ]);
  ok(editor.enable.checked, "Should be enabled");
}

async function testWidthIncrements(view) {
  info("Testing dragging the mouse on the width property");

  const marginPropEditor = getTextProperty(view, 1, { width: "0px" }).editor;
  await runIncrementTest(marginPropEditor, view, [
    {
      startValue: "0px",
      expectedEndValue: "20px",
      distance: 20,
      description: "Increasing value while dragging",
    },
    {
      startValue: "20px",
      expectedEndValue: "0px",
      distance: -20,
      description: "Decreasing value while dragging",
    },
    {
      startValue: "0px",
      expectedEndValue: "2px",
      ...getSmallIncrementKey(),
      distance: 20,
      description:
        "Increasing value with small increments by pressing ctrl or alt",
    },
    {
      startValue: "2px",
      expectedEndValue: "202px",
      shift: true,
      distance: 20,
      description: "Increasing value with large increments by pressing shift",
    },
    {
      startValue: "202px",
      expectedEndValue: "402px",
      distance: 200,
      description: "Increasing value with long distance",
    },
  ]);
}

async function testDraggingClassIsAddedOnValueUpdate(view) {
  info("Testing dragging class is added when a supported unit is detected");

  const editor = getTextProperty(view, 1, { "margin-top": "unset" }).editor;
  const valueSpan = editor.valueSpan;
  ok(
    !valueSpan.classList.contains(DRAGGABLE_VALUE_CLASSNAME),
    "Should not be draggable"
  );
  valueSpan.scrollIntoView();
  await setProperty(view, editor.prop, "23em");
  ok(
    valueSpan.classList.contains(DRAGGABLE_VALUE_CLASSNAME),
    "Should be draggable"
  );
}

/**
 * Runs each test and check whether or not the property is draggable
 *
 * @param  {CSSRuleView} view
 * @param  {Array.<{
 *   name: String,
 *   value: String,
 *   shouldBeDraggable: Boolean,
 * }>} tests
 */
function runIsDraggableTest(view, tests) {
  for (const test of tests) {
    const property = test;
    info(`Testing ${property.name} with value ${property.value}`);
    const editor = getTextProperty(view, 1, {
      [property.name]: property.value,
    }).editor;
    const valueSpan = editor.valueSpan;
    if (property.shouldBeDraggable) {
      ok(
        valueSpan.classList.contains(DRAGGABLE_VALUE_CLASSNAME),
        "Should be draggable"
      );
    } else {
      ok(
        !valueSpan.classList.contains(DRAGGABLE_VALUE_CLASSNAME),
        "Should not be draggable"
      );
    }
  }
}

/**
 * Runs each test in the tests array by synthesizing a mouse dragging
 *
 * @param  {TextPropertyEditor} editor
 * @param  {CSSRuleView} view
 * @param  {Array} tests
 */
async function runIncrementTest(editor, view, tests) {
  for (const test of tests) {
    await testIncrement(editor, test, view);
  }
  view.debounce.flush();
}

/**
 * Runs an increment test
 *
 * 1. We initialize the TextProperty value with "startValue"
 * 2. We synthesize a mouse dragging of "distance" length
 * 3. We check the value of TextProperty is equal to "expectedEndValue"
 *
 * @param  {TextPropertyEditor} editor
 * @param  {Array} options
 * @param  {String} options.startValue
 * @param  {String} options.expectedEndValue
 * @param  {Boolean} options.shift Whether or not we press the shift key
 * @param  {Number} options.distance Distance of the dragging
 * @param  {String} options.description
 * @param  {Boolean} options.ctrl Small increment key
 * @param  {Boolean} options.alt Small increment key for macosx
 * @param  {CSSRuleView} view
 */
async function testIncrement(editor, options, view) {
  info("Running subtest: " + options.description);

  editor.valueSpan.scrollIntoView();
  await setProperty(editor.ruleView, editor.prop, options.startValue);

  is(
    editor.prop.value,
    options.startValue,
    "Value initialized at " + options.startValue
  );

  const onMouseUp = once(editor.valueSpan, "mouseup");

  await synthesizeMouseDragging(editor, options.distance, options);

  // mouseup event not triggered when escape is pressed
  if (!options.escape) {
    info("Waiting mouseup");
    await onMouseUp;
    info("Received mouseup");
  }

  is(
    editor.prop.value,
    options.expectedEndValue,
    "Value changed to " + editor.prop.value
  );
}

/**
 * Synthesizes mouse dragging (mousedown + mousemove + mouseup)
 *
 * @param {TextPropertyEditor} editor
 * @param {Number} distance length of the horizontal dragging (negative if dragging left)
 * @param {Object} option
 * @param {Boolean} option.escape
 * @param {Boolean} option.alt
 * @param {Boolean} option.shift
 * @param {Boolean} option.ctrl
 */
async function synthesizeMouseDragging(editor, distance, options = {}) {
  info(`Start to synthesize mouse dragging (from ${1} to ${1 + distance})`);

  const styleWindow = editor.ruleView.styleWindow;
  const elm = editor.valueSpan;
  const startPosition = [1, 1];
  const endPosition = [startPosition[0] + distance, startPosition[1]];

  EventUtils.synthesizeMouse(
    elm,
    startPosition[0],
    startPosition[1],
    { type: "mousedown" },
    styleWindow
  );

  const onPropertyUpdated = editor.ruleView.once(
    "property-updated-by-dragging"
  );

  EventUtils.synthesizeMouse(
    elm,
    endPosition[0],
    endPosition[1],
    {
      type: "mousemove",
      shiftKey: !!options.shift,
      ctrlKey: !!options.ctrl,
      altKey: !!options.alt,
    },
    styleWindow
  );

  // We wait because the mousemove event listener is throttled to 30ms
  // in the TextPropertyEditor class
  info("waiting for event property-updated-by-dragging");
  await onPropertyUpdated;
  ok(true, "received event property-updated-by-dragging");

  if (options.escape) {
    is(
      editor.prop.value,
      options.expectedEndValueBeforeEscape,
      "testing value before pressing escape"
    );
    EventUtils.synthesizeKey("VK_ESCAPE", {}, styleWindow);
  }

  const ruleviewChanged = editor.ruleView.once("ruleview-changed");

  EventUtils.synthesizeMouse(
    elm,
    endPosition[0],
    endPosition[1],
    {
      type: "mouseup",
    },
    styleWindow
  );
  await ruleviewChanged;
  info("Finish to synthesize mouse dragging");
}

function getSmallIncrementKey() {
  if (AppConstants.platform === "macosx") {
    return { alt: true };
  }
  return { ctrl: true };
}
