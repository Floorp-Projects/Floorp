/*
 * Description of the test:
 *   Check that HSTS priming occurs correctly with mixed content when active
 *   content is blocked.
 */
'use strict';

var expected_telemetry = {
  "histograms": {
    "MIXED_CONTENT_HSTS_PRIMING_RESULT": 3,
    "MIXED_CONTENT_HSTS_PRIMING_REQUESTS": 6,
    "HSTS_UPGRADE_SOURCE": [ 0,0,1,0,0,0,0,0,0 ]
  },
  "keyed-histograms": {
    "HSTS_PRIMING_REQUEST_DURATION": {
      "success": 1,
      "failure": 2,
    },
  }
};

//jscs:disable
add_task(async function() {
  //jscs:enable
  Services.obs.addObserver(Observer, "console-api-log-event");
  Services.obs.addObserver(Observer, "http-on-examine-response");
  registerCleanupFunction(do_cleanup);

  let which = "block_active";

  SetupPrefTestEnvironment(which);
  clear_hists(expected_telemetry);

  for (let server of Object.keys(test_servers)) {
    await execute_test(server, test_settings[which].mimetype);
  }

  test_telemetry(expected_telemetry);

  SpecialPowers.popPrefEnv();
});
