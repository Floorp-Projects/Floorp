"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);

Cu.import("resource://shield-recipe-client/lib/EnvExpressions.jsm", this);
Cu.import("resource://shield-recipe-client/test/browser/Utils.jsm", this);

add_task(function* () {
  // setup
  yield TelemetryController.submitExternalPing("testfoo", {foo: 1});
  yield TelemetryController.submitExternalPing("testbar", {bar: 2});

  let val;
  // Test that basic expressions work
  val = yield EnvExpressions.eval("2+2");
  is(val, 4, "basic expression works");

  // Test that multiline expressions work
  val = yield EnvExpressions.eval(`
    2
    +
    2
  `);
  is(val, 4, "multiline expression works");

  // Test it can access telemetry
  val = yield EnvExpressions.eval("telemetry");
  is(typeof val, "object", "Telemetry is accesible");

  // Test it reads different types of telemetry
  val = yield EnvExpressions.eval("telemetry");
  is(val.testfoo.payload.foo, 1, "value 'foo' is in mock telemetry");
  is(val.testbar.payload.bar, 2, "value 'bar' is in mock telemetry");

  // Test has a date transform
  val = yield EnvExpressions.eval('"2016-04-22"|date');
  const d = new Date(Date.UTC(2016, 3, 22)); // months are 0 based
  is(val.toString(), d.toString(), "Date transform works");

  // Test dates are comparable
  const context = {someTime: Date.UTC(2016, 0, 1)};
  val = yield EnvExpressions.eval('"2015-01-01"|date < someTime', context);
  ok(val, "dates are comparable with less-than");
  val = yield EnvExpressions.eval('"2017-01-01"|date > someTime', context);
  ok(val, "dates are comparable with greater-than");

  // Test stable sample returns true for matching samples
  val = yield EnvExpressions.eval('["test"]|stableSample(1)');
  is(val, true, "Stable sample returns true for 100% sample");

  // Test stable sample returns true for matching samples
  val = yield EnvExpressions.eval('["test"]|stableSample(0)');
  is(val, false, "Stable sample returns false for 0% sample");

  // Test stable sample for known samples
  val = yield EnvExpressions.eval('["test-1"]|stableSample(0.5)');
  is(val, true, "Stable sample returns true for a known sample");
  val = yield EnvExpressions.eval('["test-4"]|stableSample(0.5)');
  is(val, false, "Stable sample returns false for a known sample");

  // Test bucket sample for known samples
  val = yield EnvExpressions.eval('["test-1"]|bucketSample(0, 5, 10)');
  is(val, true, "Bucket sample returns true for a known sample");
  val = yield EnvExpressions.eval('["test-4"]|bucketSample(0, 5, 10)');
  is(val, false, "Bucket sample returns false for a known sample");

  // Test that userId is available
  val = yield EnvExpressions.eval("normandy.userId");
  ok(Utils.UUID_REGEX.test(val), "userId available");

  // test that it pulls from the right preference
  yield SpecialPowers.pushPrefEnv({set: [["extensions.shield-recipe-client.user_id", "fake id"]]});
  val = yield EnvExpressions.eval("normandy.userId");
  Assert.equal(val, "fake id", "userId is pulled from preferences");

  // test that it merges context correctly, `userId` comes from the default context, and
  // `injectedValue` comes from us. Expect both to be on the final `normandy` object.
  val = yield EnvExpressions.eval(
    "[normandy.userId, normandy.injectedValue]",
    {normandy: {injectedValue: "injected"}});
  Assert.deepEqual(val, ["fake id", "injected"], "context is correctly merged");

  // distribution id defaults to "default"
  val = yield EnvExpressions.eval("normandy.distribution");
  Assert.equal(val, "default", "distribution has a default value");

  // distribution id is in the context
  yield SpecialPowers.pushPrefEnv({set: [["distribution.id", "funnelcake"]]});
  val = yield EnvExpressions.eval("normandy.distribution");
  Assert.equal(val, "funnelcake", "distribution is read from preferences");
});
