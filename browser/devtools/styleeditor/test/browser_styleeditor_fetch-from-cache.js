/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// A test to ensure Style Editor doesn't bybass cache when loading style sheet
// contents (bug 978688).

const TEST_URL = TEST_BASE_HTTP + "doc_uncached.html";

let test = asyncTest(function() {
  waitForExplicitFinish();

  info("Opening netmonitor");
  let tab = yield addTab("about:blank");
  let target = TargetFactory.forTab(tab);
  let toolbox = yield gDevTools.showToolbox(target, "netmonitor");
  let netmonitor = toolbox.getPanel("netmonitor");

  info("Navigating to test page");
  content.location = TEST_URL;

  info("Opening Style Editor");
  let styleeditor = yield toolbox.selectTool("styleeditor");

  info("Waiting for an editor to be selected.");
  yield styleeditor.UI.once("editor-selected");

  info("Checking Netmonitor contents.");
  let requestsForCss = 0;
  for (let item of netmonitor._view.RequestsMenu) {
    if (item.attachment.url.endsWith("doc_uncached.css")) {
      requestsForCss++;
    }
  }

  is(requestsForCss, 1,
     "Got one request for doc_uncached.css after Style Editor was loaded.");

});
