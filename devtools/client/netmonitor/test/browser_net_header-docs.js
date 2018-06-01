/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if "Learn More" links are correctly displayed
 * next to headers.
 */
add_task(async function() {
  const { tab, monitor } = await initNetMonitor(POST_DATA_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const {
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");
  const { getHeadersURL } = require("devtools/client/netmonitor/src/utils/mdn-utils");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 2);

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelectorAll(".request-list-item")[0]);

  testShowLearnMore(getSortedRequests(store.getState()).get(0));

  return teardown(monitor);

  /*
   * Tests that a "Learn More" button is only shown if
   * and only if a header is documented in MDN.
   */
  function testShowLearnMore(data) {
    const selector = ".properties-view .treeRow.stringRow";
    document.querySelectorAll(selector).forEach((rowEl, index) => {
      const headerName = rowEl.querySelectorAll(".treeLabelCell .treeLabel")[0]
                            .textContent;
      const headerDocURL = getHeadersURL(headerName);
      const learnMoreEl = rowEl.querySelectorAll(".treeValueCell .learn-more-link");

      if (headerDocURL === null) {
        ok(learnMoreEl.length === 0,
           "undocumented header does not include a \"Learn More\" button");
      } else {
        ok(learnMoreEl[0].getAttribute("title") === headerDocURL,
           "documented header includes a \"Learn More\" button with a link to MDN");
      }
    });
  }
});
