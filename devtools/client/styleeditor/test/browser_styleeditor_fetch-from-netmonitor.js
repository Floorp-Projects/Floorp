/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// A test to ensure Style Editor only issues 1 request for each stylesheet (instead of 2)
// by using the network monitor's request history (bug 1306892).

const TEST_URL = TEST_BASE_HTTP + "doc_fetch_from_netmonitor.html";

add_task(function* () {
  info("Opening netmonitor");
  let tab = yield addTab("about:blank");
  let target = TargetFactory.forTab(tab);
  let toolbox = yield gDevTools.showToolbox(target, "netmonitor");
  let monitor = toolbox.getPanel("netmonitor");
  let { store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let {
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  info("Navigating to test page");
  yield navigateTo(TEST_URL);

  info("Opening Style Editor");
  let styleeditor = yield toolbox.selectTool("styleeditor");
  let ui = styleeditor.UI;

  info("Waiting for the sources to be loaded.");
  yield ui.editors[0].getSourceEditor();
  yield ui.selectStyleSheet(ui.editors[1].styleSheet);
  yield ui.editors[1].getSourceEditor();

  info("Checking Netmonitor contents.");
  let shortRequests = [];
  let longRequests = [];
  for (let item of getSortedRequests(store.getState())) {
    if (item.url.endsWith("doc_short_string.css")) {
      shortRequests.push(item);
    }
    if (item.url.endsWith("doc_long_string.css")) {
      longRequests.push(item);
    }
  }

  is(shortRequests.length, 1,
     "Got one request for doc_short_string.css after Style Editor was loaded.");
  is(longRequests.length, 1,
     "Got one request for doc_long_string.css after Style Editor was loaded.");
});
