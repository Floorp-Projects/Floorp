/*
 * Description of the test:
 *   If the top-level domain sends the STS header but does not have
 *   includeSubdomains, HSTS priming requests should still be sent to
 *   subdomains.
 */
'use strict';

//jscs:disable
add_task(function*() {
  //jscs:enable
  Observer.add_observers(Services);
  registerCleanupFunction(do_cleanup);

  // add the top-level server
  test_servers['non-standard-port'] = {
    host: 'example.com:1234',
    response: true,
    id: 'non-standard-port',
  };
  test_settings.block_active.result['non-standard-port'] = 'blocked';

  let which = "block_active";

  SetupPrefTestEnvironment(which);

  yield execute_test("non-standard-port", test_settings[which].mimetype);

  yield execute_test("prime-hsts", test_settings[which].mimetype);

  ok("prime-hsts" in test_settings[which_test].priming, "Sent priming request on standard port after non-standard was not primed");

  SpecialPowers.popPrefEnv();
});
