/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if invalid filter types are sanitized when loaded from the preferences.
 */

function test() {
  Services.prefs.setCharPref("devtools.netmonitor.filters", '["js", "bogus"]');

  initNetMonitor(FILTERING_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { Prefs } = aMonitor.panelWin;

    is(Prefs.filters.length, 2,
      "All filter types were loaded as an array from the preferences.");
    is(Prefs.filters[0], "js",
      "The first filter type is correct.");
    is(Prefs.filters[1], "bogus",
      "The second filter type is invalid, but loaded anyway.");

    waitForNetworkEvents(aMonitor, 7).then(() => {
      testFilterButtons(aMonitor, "js");
      ok(true, "Only the correct filter type was taken into consideration.");

      teardown(aMonitor).then(() => {
        let filters = Services.prefs.getCharPref("devtools.netmonitor.filters");
        is(filters, '["js"]',
          "The bogus filter type was ignored and removed from the preferences.");

        finish();
      });
    });

    aDebuggee.performRequests('{ "getMedia": true, "getFlash": true }');
  });
}
