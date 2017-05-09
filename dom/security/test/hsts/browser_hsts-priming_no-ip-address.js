/*
 * Description of the test:
 *   If the top-level domain sends the STS header but does not have
 *   includeSubdomains, HSTS priming requests should still be sent to
 *   subdomains.
 */
'use strict';

var expected_telemetry = {
  "histograms": {
    "MIXED_CONTENT_HSTS_PRIMING_RESULT": 0,
    "MIXED_CONTENT_HSTS_PRIMING_REQUESTS": 1,
  },
  "keyed-histograms": {
  }
};

//jscs:disable
add_task(function*() {
  //jscs:enable
  Observer.add_observers(Services);
  registerCleanupFunction(do_cleanup);

  // add the top-level server
  test_servers['localhost-ip'] = {
    host: '127.0.0.2',
    response: true,
    id: 'localhost-ip',
  };
  test_settings.block_active.result['localhost-ip'] = 'blocked';

  let which = "block_active";

  SetupPrefTestEnvironment(which);
  clear_hists(expected_telemetry);

  yield execute_test("localhost-ip", test_settings[which].mimetype);

  test_telemetry(expected_telemetry);

  SpecialPowers.popPrefEnv();
});
