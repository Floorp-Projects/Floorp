/*
 * Description of the test:
 *   Only one request should be sent per host, even if we run the test more
 *   than once.
 */
'use strict';

var expected_telemetry = {
  "histograms": {
    "MIXED_CONTENT_HSTS_PRIMING_RESULT": 3,
    "MIXED_CONTENT_HSTS_PRIMING_REQUESTS": 3,
  },
  "keyed-histograms": {
    "HSTS_PRIMING_REQUEST_DURATION": {
      "failure": 3,
    },
  }
};

//jscs:disable
add_task(async function() {
  //jscs:enable
  Observer.add_observers(Services);
  registerCleanupFunction(do_cleanup);

  let which = "timeout";

  SetupPrefTestEnvironment(which, [["security.mixed_content.hsts_priming_request_timeout",
                1000]]);
  clear_hists(expected_telemetry);

  for (let server of Object.keys(test_servers)) {
    await execute_test(server, test_settings[which].mimetype);
  }

  test_telemetry(expected_telemetry);

  SpecialPowers.popPrefEnv();
});
