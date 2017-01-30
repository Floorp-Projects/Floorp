/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const HeaderDocs = require("devtools/server/actors/headerdocs");

/**
 * Tests if "Learn More" links are correctly displayed
 * next to headers.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(POST_DATA_URL);
  info("Starting test... ");

  let { document, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 0, 2);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  let origItem = RequestsMenu.getItemAtIndex(0);
  RequestsMenu.selectedItem = origItem;

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelectorAll(".request-list-item")[0]);

  testShowLearnMore(origItem);

  wait = waitForDOM(document, ".properties-view .tree-container tbody");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelectorAll(".learn-more-link")[0]);
  yield wait;

  return teardown(monitor);

  /*
   * Tests that a "Learn More" button is only shown if
   * and only if a header is documented in MDN.
   */
  function testShowLearnMore(data) {
    document.querySelectorAll(".properties-view .treeRow.stringRow").forEach((rowEl, index) => {
      let headerName = rowEl.querySelectorAll(".treeLabelCell .treeLabel")[0].textContent;
      let headerDocURL = HeaderDocs.getURL(headerName);
      let learnMoreEl = rowEl.querySelectorAll(".treeValueCell .learn-more-link");

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
