"use strict";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://shield-recipe-client/lib/ClientEnvironment.jsm", this);


add_task(function* testTelemetry() {
  // setup
  yield TelemetryController.submitExternalPing("testfoo", {foo: 1});
  yield TelemetryController.submitExternalPing("testbar", {bar: 2});
  const environment = ClientEnvironment.getEnvironment();

  // Test it can access telemetry
  const telemetry = yield environment.telemetry;
  is(typeof telemetry, "object", "Telemetry is accessible");

  // Test it reads different types of telemetry
  is(telemetry.testfoo.payload.foo, 1, "value 'foo' is in mock telemetry");
  is(telemetry.testbar.payload.bar, 2, "value 'bar' is in mock telemetry");
});

add_task(function* testUserId() {
  let environment = ClientEnvironment.getEnvironment();

  // Test that userId is available
  ok(UUID_REGEX.test(environment.userId), "userId available");

  // test that it pulls from the right preference
  yield SpecialPowers.pushPrefEnv({set: [["extensions.shield-recipe-client.user_id", "fake id"]]});
  environment = ClientEnvironment.getEnvironment();
  is(environment.userId, "fake id", "userId is pulled from preferences");
});

add_task(function* testDistribution() {
  let environment = ClientEnvironment.getEnvironment();

  // distribution id defaults to "default"
  is(environment.distribution, "default", "distribution has a default value");

  // distribution id is read from a preference
  yield SpecialPowers.pushPrefEnv({set: [["distribution.id", "funnelcake"]]});
  environment = ClientEnvironment.getEnvironment();
  is(environment.distribution, "funnelcake", "distribution is read from preferences");
});

const mockClassify = {country: "FR", request_time: new Date(2017, 1, 1)};
add_task(ClientEnvironment.withMockClassify(mockClassify, function* testCountryRequestTime() {
  const environment = ClientEnvironment.getEnvironment();

  // Test that country and request_time pull their data from the server.
  is(yield environment.country, mockClassify.country, "country is read from the server API");
  is(
    yield environment.request_time, mockClassify.request_time,
    "request_time is read from the server API"
  );
}));

add_task(function* testSync() {
  let environment = ClientEnvironment.getEnvironment();
  is(environment.syncMobileDevices, 0, "syncMobileDevices defaults to zero");
  is(environment.syncDesktopDevices, 0, "syncDesktopDevices defaults to zero");
  is(environment.syncTotalDevices, 0, "syncTotalDevices defaults to zero");
  yield SpecialPowers.pushPrefEnv({
    set: [
      ["services.sync.numClients", 9],
      ["services.sync.clients.devices.mobile", 5],
      ["services.sync.clients.devices.desktop", 4],
    ],
  });
  environment = ClientEnvironment.getEnvironment();
  is(environment.syncMobileDevices, 5, "syncMobileDevices is read when set");
  is(environment.syncDesktopDevices, 4, "syncDesktopDevices is read when set");
  is(environment.syncTotalDevices, 9, "syncTotalDevices is read when set");
});

add_task(function* testDoNotTrack() {
  let environment = ClientEnvironment.getEnvironment();

  // doNotTrack defaults to false
  ok(!environment.doNotTrack, "doNotTrack has a default value");

  // doNotTrack is read from a preference
  yield SpecialPowers.pushPrefEnv({set: [["privacy.donottrackheader.enabled", true]]});
  environment = ClientEnvironment.getEnvironment();
  ok(environment.doNotTrack, "doNotTrack is read from preferences");
});
