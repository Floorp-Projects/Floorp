"use strict";

Cu.import("resource://shield-recipe-client/lib/FilterExpressions.jsm", this);

add_task(function* () {
  let val;
  // Test that basic expressions work
  val = yield FilterExpressions.eval("2+2");
  is(val, 4, "basic expression works");

  // Test that multiline expressions work
  val = yield FilterExpressions.eval(`
    2
    +
    2
  `);
  is(val, 4, "multiline expression works");

  // Test has a date transform
  val = yield FilterExpressions.eval('"2016-04-22"|date');
  const d = new Date(Date.UTC(2016, 3, 22)); // months are 0 based
  is(val.toString(), d.toString(), "Date transform works");

  // Test dates are comparable
  const context = {someTime: Date.UTC(2016, 0, 1)};
  val = yield FilterExpressions.eval('"2015-01-01"|date < someTime', context);
  ok(val, "dates are comparable with less-than");
  val = yield FilterExpressions.eval('"2017-01-01"|date > someTime', context);
  ok(val, "dates are comparable with greater-than");

  // Test stable sample returns true for matching samples
  val = yield FilterExpressions.eval('["test"]|stableSample(1)');
  is(val, true, "Stable sample returns true for 100% sample");

  // Test stable sample returns true for matching samples
  val = yield FilterExpressions.eval('["test"]|stableSample(0)');
  is(val, false, "Stable sample returns false for 0% sample");

  // Test stable sample for known samples
  val = yield FilterExpressions.eval('["test-1"]|stableSample(0.5)');
  is(val, true, "Stable sample returns true for a known sample");
  val = yield FilterExpressions.eval('["test-4"]|stableSample(0.5)');
  is(val, false, "Stable sample returns false for a known sample");

  // Test bucket sample for known samples
  val = yield FilterExpressions.eval('["test-1"]|bucketSample(0, 5, 10)');
  is(val, true, "Bucket sample returns true for a known sample");
  val = yield FilterExpressions.eval('["test-4"]|bucketSample(0, 5, 10)');
  is(val, false, "Bucket sample returns false for a known sample");

  // Test that it reads from the context correctly.
  val = yield FilterExpressions.eval("first + second + 3", {first: 1, second: 2});
  is(val, 6, "context is available to filter expressions");
});
