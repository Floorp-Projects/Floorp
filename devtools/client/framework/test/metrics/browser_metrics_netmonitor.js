/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test records the number of modules loaded by DevTools, as well as the total count
 * of characters in those modules, when opening the netmonitor. These metrics are
 * retrieved by perfherder via logs.
 */

const TEST_URL =
  "data:text/html;charset=UTF-8,<div>Netmonitor modules load test</div>";

add_task(async function() {
  const toolbox = await openNewTabAndToolbox(TEST_URL, "netmonitor");
  const panel = toolbox.getCurrentPanel();

  // Retrieve the browser loader dedicated to the Netmonitor.
  const netmonitorLoader = panel.panelWin.getBrowserLoaderForWindow();
  const loaders = [loader.loader, netmonitorLoader.loader];

  // Uncomment after Bug 1581068 is fixed, otherwise the test might fail too
  // frequently.

  // const whitelist = [
  //   "@loader/unload.js",
  //   "@loader/options.js",
  //   "chrome.js",
  //   "resource://devtools/client/netmonitor/src/api.js",
  //   "resource://devtools/client/shared/vendor/redux.js",
  //   "resource://devtools/client/netmonitor/src/connector/index.js",
  //   "resource://devtools/client/netmonitor/src/create-store.js",
  //   "resource://devtools/client/netmonitor/src/constants.js",
  //   "resource://devtools/client/netmonitor/src/middleware/batching.js",
  //   "resource://devtools/client/netmonitor/src/middleware/prefs.js",
  //   "resource://devtools/client/netmonitor/src/middleware/thunk.js",
  //   "resource://devtools/client/netmonitor/src/middleware/recording.js",
  //   "resource://devtools/client/netmonitor/src/selectors/index.js",
  //   "resource://devtools/client/netmonitor/src/selectors/requests.js",
  //   "resource://devtools/client/shared/vendor/reselect.js",
  //   "resource://devtools/client/netmonitor/src/utils/filter-predicates.js",
  //   "resource://devtools/client/netmonitor/src/utils/filter-text-utils.js",
  //   "resource://devtools/client/netmonitor/src/utils/format-utils.js",
  //   "resource://devtools/client/netmonitor/src/utils/l10n.js",
  //   "resource://devtools/client/netmonitor/src/utils/sort-predicates.js",
  //   "resource://devtools/client/netmonitor/src/utils/request-utils.js",
  //   "resource://devtools/client/netmonitor/src/selectors/search.js",
  //   "resource://devtools/client/netmonitor/src/selectors/timing-markers.js",
  //   "resource://devtools/client/netmonitor/src/selectors/ui.js",
  //   "resource://devtools/client/netmonitor/src/selectors/web-sockets.js",
  //   "resource://devtools/client/netmonitor/src/middleware/throttling.js",
  //   "resource://devtools/client/shared/components/throttling/actions.js",
  //   "resource://devtools/client/netmonitor/src/middleware/event-telemetry.js",
  //   "resource://devtools/client/netmonitor/src/reducers/index.js",
  //   "resource://devtools/client/netmonitor/src/reducers/batching.js",
  //   "resource://devtools/client/netmonitor/src/reducers/requests.js",
  //   "resource://devtools/client/netmonitor/src/reducers/search.js",
  //   "resource://devtools/client/netmonitor/src/reducers/sort.js",
  //   "resource://devtools/client/netmonitor/src/reducers/filters.js",
  //   "resource://devtools/client/netmonitor/src/reducers/timing-markers.js",
  //   "resource://devtools/client/netmonitor/src/reducers/ui.js",
  //   "resource://devtools/client/netmonitor/src/reducers/web-sockets.js",
  //   "resource://devtools/client/shared/components/throttling/reducer.js",
  //   "resource://devtools/client/netmonitor/src/actions/index.js",
  //   "resource://devtools/client/netmonitor/src/actions/batching.js",
  //   "resource://devtools/client/netmonitor/src/actions/filters.js",
  //   "resource://devtools/client/netmonitor/src/actions/requests.js",
  //   "resource://devtools/client/netmonitor/src/actions/selection.js",
  //   "resource://devtools/client/netmonitor/src/actions/sort.js",
  //   "resource://devtools/client/netmonitor/src/actions/timing-markers.js",
  //   "resource://devtools/client/netmonitor/src/actions/ui.js",
  //   "resource://devtools/client/netmonitor/src/actions/web-sockets.js",
  //   "resource://devtools/client/netmonitor/src/actions/search.js",
  //   "resource://devtools/client/netmonitor/src/workers/search/index.js",
  //   "resource://devtools/client/netmonitor/src/workers/worker-utils.js",
  // ];
  // runDuplicatedModulesTest(loaders, whitelist);

  runMetricsTest({
    filterString: "devtools/client/netmonitor",
    loaders,
    panelName: "netmonitor",
  });
});
