/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function getSimpleMeasurementsFromTelemetryPing() {
  const TelemetryPing = Cc["@mozilla.org/base/telemetry-ping;1"].getService(Ci.nsITelemetryPing);
  let ping = TelemetryPing.getPayload();

  return ping.simpleMeasurements;
}

function test() {
  waitForExplicitFinish()
  const Telemetry = Services.telemetry;
  Telemetry.asyncReadShutdownTime(function () {
    actualTest();
    finish();
  });
}

function actualTest() {
  // Test the module logic
  let tmp = {};
  Cu.import("resource:///modules/TelemetryTimestamps.jsm", tmp);
  let TelemetryTimestamps = tmp.TelemetryTimestamps;
  let now = Date.now();
  TelemetryTimestamps.add("foo");
  ok(TelemetryTimestamps.get().foo, "foo was added");
  ok(TelemetryTimestamps.get().foo >= now, "foo has a reasonable value");

  // Add timestamp with value
  // Use a value far in the future since TelemetryPing substracts the time of
  // process initialization.
  const YEAR_4000_IN_MS = 64060588800000;
  TelemetryTimestamps.add("bar", YEAR_4000_IN_MS);
  ok(TelemetryTimestamps.get().bar, "bar was added");
  is(TelemetryTimestamps.get().bar, YEAR_4000_IN_MS, "bar has the right value");

  // Can't add the same timestamp twice
  TelemetryTimestamps.add("bar", 2);
  is(TelemetryTimestamps.get().bar, YEAR_4000_IN_MS, "bar wasn't overwritten");

  let threw = false;
  try {
    TelemetryTimestamps.add("baz", "this isn't a number");
  } catch (ex) {
    threw = true;
  }
  ok(threw, "adding baz threw");
  ok(!TelemetryTimestamps.get().baz, "no baz was added");

  // Test that the data gets added to the telemetry ping properly
  let simpleMeasurements = getSimpleMeasurementsFromTelemetryPing();
  ok(simpleMeasurements, "got simple measurements from ping data");
  ok(simpleMeasurements.foo > 1, "foo was included");
  ok(simpleMeasurements.bar > 1, "bar was included");
  ok(!simpleMeasurements.baz, "baz wasn't included since it wasn't added");

  // Check browser timestamps that we add
  let props = [
    // These can't be reliably tested when the test is run alone
    //"delayedStartupStarted",
    //"delayedStartupFinished",
    "sessionRestoreInitialized",
    // This doesn't get hit in the testing profile
    //"sessionRestoreRestoring"
  ];

  props.forEach(function (p) {
    let value = simpleMeasurements[p];
    ok(value, p + " exists");
    ok(!isNaN(value), p + " is a number");
    ok(value > 0, p + " value is reasonable");
  });
}
