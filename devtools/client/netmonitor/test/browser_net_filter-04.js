/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if invalid filter types are sanitized when loaded from the preferences.
 */

const BASIC_REQUESTS = [
  { url: "sjs_content-type-test-server.sjs?fmt=html&res=undefined" },
  { url: "sjs_content-type-test-server.sjs?fmt=css" },
  { url: "sjs_content-type-test-server.sjs?fmt=js" },
];

const REQUESTS_WITH_MEDIA = BASIC_REQUESTS.concat([
  { url: "sjs_content-type-test-server.sjs?fmt=font" },
  { url: "sjs_content-type-test-server.sjs?fmt=image" },
  { url: "sjs_content-type-test-server.sjs?fmt=audio" },
  { url: "sjs_content-type-test-server.sjs?fmt=video" },
]);

const REQUESTS_WITH_MEDIA_AND_FLASH = REQUESTS_WITH_MEDIA.concat([
  { url: "sjs_content-type-test-server.sjs?fmt=flash" },
]);

function test() {
  Services.prefs.setCharPref("devtools.netmonitor.filters", '["js", "bogus"]');

  initNetMonitor(FILTERING_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { Prefs, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    is(Prefs.filters.length, 2,
      "All filter types were loaded as an array from the preferences.");
    is(Prefs.filters[0], "js",
      "The first filter type is correct.");
    is(Prefs.filters[1], "bogus",
      "The second filter type is invalid, but loaded anyway.");

    waitForNetworkEvents(aMonitor, 8).then(() => {
      testFilterButtons(aMonitor, "js");
      ok(true, "Only the correct filter type was taken into consideration.");

      teardown(aMonitor).then(() => {
        let filters = Services.prefs.getCharPref("devtools.netmonitor.filters");
        is(filters, '["js"]',
          "The bogus filter type was ignored and removed from the preferences.");

        finish();
      });
    });

    loadCommonFrameScript();
    performRequestsInContent(REQUESTS_WITH_MEDIA_AND_FLASH);
  });
}
