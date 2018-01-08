/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// A test to ensure Style Editor doesn't bybass cache when loading style sheet
// contents (bug 978688).

const TEST_URL = TEST_BASE_HTTP + "doc_uncached.html";

add_task(function* () {
  // Disable rcwn to make cache behavior deterministic.
  yield pushPref("network.http.rcwn.enabled", false);

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

  info("Waiting for the source to be loaded.");
  yield styleeditor.UI.editors[0].getSourceEditor();

  info("Checking Netmonitor contents.");
  let items = [];
  for (let item of getSortedRequests(store.getState())) {
    if (item.url.endsWith("doc_uncached.css")) {
      items.push(item);
    }
  }

  is(items.length, 1,
     "Got one request for doc_uncached.css after Style Editor was loaded.");
});
