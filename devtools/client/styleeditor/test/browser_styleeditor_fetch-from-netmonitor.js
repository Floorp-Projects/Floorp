/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// A test to ensure Style Editor only issues 1 request for each stylesheet (instead of 2)
// by using the network monitor's request history (bug 1306892).

const TEST_URL = TEST_BASE_HTTP + "doc_fetch_from_netmonitor.html";

add_task(async function() {
  info("Opening netmonitor");
  const tab = await addTab("about:blank");
  const target = TargetFactory.forTab(tab);
  const toolbox = await gDevTools.showToolbox(target, "netmonitor");
  const monitor = toolbox.getPanel("netmonitor");
  const { store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const {
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  info("Navigating to test page");
  await navigateTo(TEST_URL);

  info("Opening Style Editor");
  const styleeditor = await toolbox.selectTool("styleeditor");
  const ui = styleeditor.UI;

  info("Waiting for the sources to be loaded.");
  await ui.editors[0].getSourceEditor();
  await ui.selectStyleSheet(ui.editors[1].styleSheet);
  await ui.editors[1].getSourceEditor();

  // Wait till there is 5 requests in Netmonitor store.
  // (i.e. the Styleeditor panel performed one request).
  await waitUntil(() => getSortedRequests(store.getState()).length == 5);

  info("Checking Netmonitor contents.");
  const shortRequests = [];
  const longRequests = [];
  const hugeRequests = [];
  for (const item of getSortedRequests(store.getState())) {
    if (item.url.endsWith("doc_short_string.css")) {
      shortRequests.push(item);
    }
    if (item.url.endsWith("doc_long_string.css")) {
      longRequests.push(item);
    }
    if (item.url.endsWith("sjs_huge-css-server.sjs")) {
      hugeRequests.push(item);
    }
  }

  is(shortRequests.length, 1,
     "Got one request for doc_short_string.css after Style Editor was loaded.");
  is(longRequests.length, 1,
     "Got one request for doc_long_string.css after Style Editor was loaded.");

  // Requests with a response body size greater than 1MB cannot be fetched from the
  // netmonitor, the style editor should perform a separate request.
  is(hugeRequests.length, 2,
     "Got two requests for sjs_huge-css-server.sjs after Style Editor was loaded.");
});
