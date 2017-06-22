/*
 * Description of the test:
 *   If the top-level domain sends the STS header but does not have
 *   includeSubdomains, HSTS priming requests should still be sent to
 *   subdomains.
 */
'use strict';

var expected_telemetry = {
  "histograms": {
    "MIXED_CONTENT_HSTS_PRIMING_RESULT": 1,
    "MIXED_CONTENT_HSTS_PRIMING_REQUESTS": 3,
    "HSTS_UPGRADE_SOURCE": [ 0,0,1,0,0,0,0,0,0 ]
  },
  "keyed-histograms": {
    "HSTS_PRIMING_REQUEST_DURATION": {
      "success": 1,
    },
  }
};

//jscs:disable
add_task(async function() {
  //jscs:enable
  Observer.add_observers(Services);
  registerCleanupFunction(do_cleanup);

  // add the top-level server
  test_servers['non-standard-port'] = {
    host: 'test1.example.com:1234',
    response: true,
    id: 'non-standard-port',
  };
  test_settings.block_active.result['non-standard-port'] = 'blocked';

  let which = "block_active";

  SetupPrefTestEnvironment(which);
  clear_hists(expected_telemetry);

  await execute_test("non-standard-port", test_settings[which].mimetype);

  await execute_test("prime-hsts", test_settings[which].mimetype);

  ok("prime-hsts" in test_settings[which_test].priming, "Sent priming request on standard port after non-standard was not primed");

  test_telemetry(expected_telemetry);

  SpecialPowers.popPrefEnv();
});
