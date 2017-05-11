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
  test_servers['localhost-ip'] = {
    host: '127.0.0.2',
    response: true,
    id: 'localhost-ip',
  };
  test_settings.block_active.result['localhost-ip'] = 'blocked';

  let which = "block_active";

  SetupPrefTestEnvironment(which);

  yield execute_test("localhost-ip", test_settings[which].mimetype);

  SpecialPowers.popPrefEnv();
});
