/*
 * Description of the test:
 * Test that the network.hsts_priming.cache_timeout preferene causes the cache
 * to timeout
 */
'use strict';

var expected_telemetry = {
  "histograms": {
    "MIXED_CONTENT_HSTS_PRIMING_RESULT": 2,
    "MIXED_CONTENT_HSTS_PRIMING_REQUESTS": 4,
  },
  "keyed-histograms": {
    "HSTS_PRIMING_REQUEST_DURATION": {
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

  SetupPrefTestEnvironment(which, [["security.mixed_content.hsts_priming_cache_timeout", 1]]);
  clear_hists(expected_telemetry);

  yield execute_test("no-ssl", test_settings[which].mimetype);

  let pre_promise = performance.now();

  while ((performance.now() - pre_promise) < 2000) {
    yield new Promise(function (resolve) {
      setTimeout(resolve, 2000);
    });
  }

  // clear the fact that we saw a priming request
  test_settings[which].priming = {};

  yield execute_test("no-ssl", test_settings[which].mimetype);
  is(test_settings[which].priming["no-ssl"], true,
    "Correctly send a priming request after expiration.");

  test_telemetry(expected_telemetry);

  SpecialPowers.popPrefEnv();
});
