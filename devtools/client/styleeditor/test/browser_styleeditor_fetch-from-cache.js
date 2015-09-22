/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// A test to ensure Style Editor doesn't bybass cache when loading style sheet
// contents (bug 978688).

const TEST_URL = TEST_BASE_HTTP + "doc_uncached.html";

add_task(function*() {
  info("Opening netmonitor");
  let tab = yield addTab("about:blank");
  let target = TargetFactory.forTab(tab);
  let toolbox = yield gDevTools.showToolbox(target, "netmonitor");
  let netmonitor = toolbox.getPanel("netmonitor");
  netmonitor._view.RequestsMenu.lazyUpdate = false;

  info("Navigating to test page");
  yield navigateTo(TEST_URL);

  info("Opening Style Editor");
  let styleeditor = yield toolbox.selectTool("styleeditor");

  info("Waiting for the source to be loaded.");
  yield styleeditor.UI.editors[0].getSourceEditor();

  info("Checking Netmonitor contents.");
  let attachments = [];
  for (let item of netmonitor._view.RequestsMenu) {
    if (item.attachment.url.endsWith("doc_uncached.css")) {
      attachments.push(item.attachment);
    }
  }

  is(attachments.length, 2,
     "Got two requests for doc_uncached.css after Style Editor was loaded.");
  ok(attachments[1].fromCache,
     "Second request was loaded from browser cache");
});
