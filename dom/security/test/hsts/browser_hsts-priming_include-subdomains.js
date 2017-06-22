/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Description of the test:
 *   If the top-level domain sends the STS header but does not have
 *   includeSubdomains, HSTS priming requests should still be sent to
 *   subdomains.
 */
'use strict';

var expected_telemetry = {
  "histograms": {
    "MIXED_CONTENT_HSTS_PRIMING_RESULT": 2,
    "MIXED_CONTENT_HSTS_PRIMING_REQUESTS": 4,
    "HSTS_UPGRADE_SOURCE": [ 0,0,2,0,0,0,0,0,0 ]
  },
  "keyed-histograms": {
    "HSTS_PRIMING_REQUEST_DURATION": {
      "success": 2,
    },
  }
};

//jscs:disable
add_task(async function() {
  //jscs:enable
  Observer.add_observers(Services);
  registerCleanupFunction(do_cleanup);

  // add the top-level server
  test_servers['top-level'] = {
    host: 'example.com',
    response: true,
    id: 'top-level',
  };
  test_settings.block_active.result['top-level'] = 'secure';

  let which = "block_active";

  SetupPrefTestEnvironment(which);
  clear_hists(expected_telemetry);

  await execute_test("top-level", test_settings[which].mimetype);

  await execute_test("prime-hsts", test_settings[which].mimetype);

  ok("prime-hsts" in test_settings[which].priming,
     "HSTS priming on a subdomain when top-level does not includeSubDomains");

  test_telemetry(expected_telemetry);

  SpecialPowers.popPrefEnv();
});
