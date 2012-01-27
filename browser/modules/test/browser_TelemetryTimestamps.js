/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function getSimpleMeasurementsFromTelemetryPing() {
  const TelemetryPing = Cc["@mozilla.org/base/telemetry-ping;1"].getService(Ci.nsIObserver);
  let str = Cc['@mozilla.org/supports-string;1'].createInstance(Ci.nsISupportsString);
  TelemetryPing.observe(str, "get-payload", "");

  return JSON.parse(str.data).simpleMeasurements;
}

function test() {
  // Test the module logic
  Cu.import("resource:///modules/TelemetryTimestamps.jsm");
  let now = Date.now();
  TelemetryTimestamps.add("foo");
  let fooValue = TelemetryTimestamps.get().foo;
  ok(fooValue, "foo was added");
  ok(fooValue >= now, "foo has a reasonable value");

  // Add timestamp with value
  TelemetryTimestamps.add("bar", 1);
  ok(TelemetryTimestamps.get().bar, "bar was added");
  is(TelemetryTimestamps.get().bar, 1, "bar has the right value");

  // Can't add the same timestamp twice
  TelemetryTimestamps.add("bar", 2);
  is(TelemetryTimestamps.get().bar, 1, "bar wasn't overwritten");

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
  is(simpleMeasurements.foo, fooValue, "foo was included");
  is(simpleMeasurements.bar, 1, "bar was included");
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
    ok(value > 0 && value < Date.now(), p + " value is reasonable");
  });
}
