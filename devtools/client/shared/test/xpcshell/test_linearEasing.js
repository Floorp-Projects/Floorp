/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests methods from the LinearEasingWidget module

const {
  LinearEasingFunctionWidget,
  parseTimingFunction,
} = require("resource://devtools/client/shared/widgets/LinearEasingFunctionWidget.js");

add_task(function testParseTimingFunction() {
  info("test parseTimingFunction");

  for (const test of ["ease", "linear", "ease-in", "ease-out", "ease-in-out"]) {
    ok(!parseTimingFunction(test), `"${test}" is not valid`);
  }

  ok(!parseTimingFunction("something"), "non-function token");
  ok(!parseTimingFunction("something()"), "non-linear function");
  ok(
    !parseTimingFunction(
      "linear(something)",
      "linear with non-numeric argument"
    )
  );

  ok(!parseTimingFunction("linear(0)", "linear with only 1 point"));

  deepEqual(
    parseTimingFunction("linear(0, 0.5, 1)"),
    [
      { input: 0, output: 0 },
      { input: 0.5, output: 0.5 },
      { input: 1, output: 1 },
    ],
    "correct invocation"
  );
  deepEqual(
    parseTimingFunction("linear(0, 0.5 /* mid */, 1)"),
    [
      { input: 0, output: 0 },
      { input: 0.5, output: 0.5 },
      { input: 1, output: 1 },
    ],
    "correct with comments and whitespace"
  );
  deepEqual(
    parseTimingFunction("linear(0 10%, 0.5 20%, 1 90%)"),
    [
      { input: 0.1, output: 0 },
      { input: 0.2, output: 0.5 },
      { input: 0.9, output: 1 },
    ],
    "correct invocation with single stop"
  );
  deepEqual(
    parseTimingFunction(
      "linear(0, 0.1, 0.2, 0.3, 0.4, 0.5 50%, 0.6, 0.7, 0.8, 0.9 70%, 1)"
    ),
    [
      { input: 0, output: 0 },
      { input: 0.1, output: 0.1 },
      { input: 0.2, output: 0.2 },
      { input: 0.3, output: 0.3 },
      { input: 0.4, output: 0.4 },
      { input: 0.5, output: 0.5 },
      { input: 0.55, output: 0.6 },
      { input: 0.6, output: 0.7 },
      {
        // This should be 0.65, but JS doesn't play well with floating points, which makes
        // the test fail. So re-do the computation here
        input: 0.5 * 0.25 + 0.7 * 0.75,
        output: 0.8,
      },
      { input: 0.7, output: 0.9 },
      { input: 1, output: 1 },
    ],
    "correct invocation with single stop and run of non-stop values"
  );

  deepEqual(
    parseTimingFunction("linear(0, 0.5 80%, 0.75 40%, 1)"),
    [
      { input: 0, output: 0 },
      { input: 0.8, output: 0.5 },
      { input: 0.8, output: 0.75 },
      { input: 1, output: 1 },
    ],
    "correct invocation with out of order single stop"
  );

  deepEqual(
    parseTimingFunction("linear(0.5 10% 40%, 0, 0.2 60% 70%, 0.75 80% 100%)"),
    [
      { input: 0.1, output: 0.5 },
      { input: 0.4, output: 0.5 },
      { input: 0.5, output: 0 },
      { input: 0.6, output: 0.2 },
      { input: 0.7, output: 0.2 },
      { input: 0.8, output: 0.75 },
      { input: 1, output: 0.75 },
    ],
    "correct invocation with multiple stops"
  );

  deepEqual(
    parseTimingFunction("linear(0, 0.2 60% 10%, 1)"),
    [
      { input: 0, output: 0 },
      { input: 0.6, output: 0.2 },
      { input: 0.6, output: 0.2 },
      { input: 1, output: 1 },
    ],
    "correct invocation with multiple out of order stops"
  );

  deepEqual(
    parseTimingFunction("linear(0, 1.5, 1)"),
    [
      { input: 0, output: 0 },
      { input: 0.5, output: 1.5 },
      { input: 1, output: 1 },
    ],
    "linear function easing with output greater than 1"
  );

  deepEqual(
    parseTimingFunction("linear(1, -0.5, 0)"),
    [
      { input: 0, output: 1 },
      { input: 0.5, output: -0.5 },
      { input: 1, output: 0 },
    ],
    "linear function easing with output less than 1"
  );

  deepEqual(
    parseTimingFunction("linear(0, 0.1 -10%, 1)"),
    [
      { input: 0, output: 0 },
      { input: 0, output: 0.1 },
      { input: 1, output: 1 },
    ],
    "correct invocation, input value being unspecified in the first entry implies zero"
  );

  deepEqual(
    parseTimingFunction("linear(0, 0.9 110%, 1)"),
    [
      { input: 0, output: 0 },
      { input: 1.1, output: 0.9 },
      { input: 1.1, output: 1 },
    ],
    "correct invocation, input value being unspecified in the last entry implies max input value"
  );
});

add_task(function testGetSetCssLinearValue() {
  const doc = Services.appShell.createWindowlessBrowser().document;
  const widget = new LinearEasingFunctionWidget(doc.body);

  widget.setCssLinearValue("linear(0)");
  ok(!widget.getCssLinearValue(), "no value returned for invalid value");

  widget.setCssLinearValue("linear(0, 0.5, 1)");
  deepEqual(
    widget.getCssLinearValue(),
    "linear(0 0%, 0.5 50%, 1 100%)",
    "no stops"
  );

  widget.setCssLinearValue("linear(0 10%, 0.5 20%, 1 90%)");
  deepEqual(
    widget.getCssLinearValue(),
    "linear(0 10%, 0.5 20%, 1 90%)",
    "with single stops"
  );

  widget.setCssLinearValue("linear(0, 0.5 80%, 0.75 40%, 1)");
  deepEqual(
    widget.getCssLinearValue(),
    "linear(0 0%, 0.5 80%, 0.75 80%, 1 100%)",
    "correcting out of order single stops"
  );

  widget.setCssLinearValue(
    "linear(0.5 10% 40%, 0, 0.2 60% 70%, 0.75 80% 100%)"
  );
  deepEqual(
    widget.getCssLinearValue(),
    "linear(0.5 10%, 0.5 40%, 0 50%, 0.2 60%, 0.2 70%, 0.75 80%, 0.75 100%)",
    "multiple stops"
  );

  widget.setCssLinearValue("linear(0, 0.2 60% 10%, 1)");
  deepEqual(
    widget.getCssLinearValue(),
    "linear(0 0%, 0.2 60%, 0.2 60%, 1 100%)",
    "correcting multiple out-of-order stops"
  );

  widget.setCssLinearValue("linear(1, -0.5, 1.5)");
  deepEqual(
    widget.getCssLinearValue(),
    "linear(1 0%, -0.5 50%, 1.5 100%)",
    "output outside of [0,1] range"
  );

  widget.setCssLinearValue("linear(0 -10%, 0.5, 1 130%)");
  deepEqual(
    widget.getCssLinearValue(),
    "linear(0 -10%, 0.5 60%, 1 130%)",
    "input outside of [0%,100%] range"
  );
});
