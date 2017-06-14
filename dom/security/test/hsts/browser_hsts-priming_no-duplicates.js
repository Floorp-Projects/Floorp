/*
 * Description of the test:
 *   Only one request should be sent per host, even if we run the test more
 *   than once.
 */
'use strict';

var expected_telemetry = {
  "histograms": {
    "MIXED_CONTENT_HSTS_PRIMING_RESULT": 3,
    "MIXED_CONTENT_HSTS_PRIMING_REQUESTS": 8,
  },
  "keyed-histograms": {
    "HSTS_PRIMING_REQUEST_DURATION": {
      "success": 1,
      "failure": 2,
    },
  }
};

//jscs:disable
add_task(function*() {
  //jscs:enable
  Observer.add_observers(Services);
  registerCleanupFunction(do_cleanup);

  let which = "block_display";

  SetupPrefTestEnvironment(which);
  clear_hists(expected_telemetry);

  for (let server of Object.keys(test_servers)) {
    yield execute_test(server, test_settings[which].mimetype);
  }

  // run the tests twice to validate the cache is being used
  for (let server of Object.keys(test_servers)) {
    yield execute_test(server, test_settings[which].mimetype);
  }

  test_telemetry(expected_telemetry);

  SpecialPowers.popPrefEnv();
});
