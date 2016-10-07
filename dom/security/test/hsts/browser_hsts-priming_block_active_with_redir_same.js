/*
 * Description of the test:
 *   Check that HSTS priming occurs correctly with mixed content when active
 *   content is blocked and redirect to the same host should still upgrade.
 */
'use strict';

//jscs:disable
add_task(function*() {
  //jscs:enable
  Services.obs.addObserver(Observer, "console-api-log-event", false);
  Services.obs.addObserver(Observer, "http-on-examine-response", false);
  registerCleanupFunction(do_cleanup);

  let which = "block_active_with_redir_same";

  SetupPrefTestEnvironment(which);

  for (let server of Object.keys(test_servers)) {
    yield execute_test(server, test_settings[which].mimetype);
  }

  SpecialPowers.popPrefEnv();
});
