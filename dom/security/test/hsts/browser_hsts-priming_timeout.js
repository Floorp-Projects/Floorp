/*
 * Description of the test:
 *   Only one request should be sent per host, even if we run the test more
 *   than once.
 */
'use strict';

//jscs:disable
add_task(function*() {
  //jscs:enable
  Observer.add_observers(Services);
  registerCleanupFunction(do_cleanup);

  let which = "timeout";

  SetupPrefTestEnvironment(which, [["security.mixed_content.hsts_priming_request_timeout",
                1000]]);

  for (let server of Object.keys(test_servers)) {
    yield execute_test(server, test_settings[which].mimetype);
  }

  SpecialPowers.popPrefEnv();
});
