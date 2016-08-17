/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the start and end buttons on the breadcrumb trail bring the right
// crumbs into the visible area, for both LTR and RTL

let { Toolbox } = require("devtools/client/framework/toolbox");

const TEST_URI = URL_ROOT + "doc_inspector_breadcrumbs_visibility.html";
const NODE_ONE = "div#aVeryLongIdToExceedTheBreadcrumbTruncationLimit";
const NODE_TWO = "div#anotherVeryLongIdToExceedTheBreadcrumbTruncationLimit";
const NODE_THREE = "div#aThirdVeryLongIdToExceedTheTruncationLimit";
const NODE_FOUR = "div#aFourthOneToExceedTheTruncationLimit";
const NODE_FIVE = "div#aFifthOneToExceedTheTruncationLimit";
const NODE_SIX = "div#aSixthOneToExceedTheTruncationLimit";
const NODE_SEVEN = "div#aSeventhOneToExceedTheTruncationLimit";

const NODES = [
  { action: "start", title: NODE_SIX },
  { action: "start", title: NODE_FIVE },
  { action: "start", title: NODE_FOUR },
  { action: "start", title: NODE_THREE },
  { action: "start", title: NODE_TWO },
  { action: "start", title: NODE_ONE },
  { action: "end", title: NODE_TWO },
  { action: "end", title: NODE_THREE },
  { action: "end", title: NODE_FOUR },
  { action: "end", title: NODE_FIVE },
  { action: "end", title: NODE_SIX }
];

add_task(function* () {
  let { inspector, toolbox } = yield openInspectorForURL(TEST_URI);

  // No way to wait for scrolling to end (Bug 1172171)
  // Rather than wait a max time; limit test to instant scroll behavior
  inspector.breadcrumbs.arrowScrollBox.scrollBehavior = "instant";

  yield toolbox.switchHost(Toolbox.HostType.WINDOW);
  let hostWindow = toolbox._host._window;
  let originalWidth = hostWindow.outerWidth;
  let originalHeight = hostWindow.outerHeight;
  hostWindow.resizeTo(640, 300);

  info("Testing transitions ltr");
  yield pushPref("intl.uidirection.en-US", "ltr");
  yield testBreadcrumbTransitions(hostWindow, inspector);

  info("Testing transitions rtl");
  yield pushPref("intl.uidirection.en-US", "rtl");
  yield testBreadcrumbTransitions(hostWindow, inspector);

  hostWindow.resizeTo(originalWidth, originalHeight);
});

function* testBreadcrumbTransitions(hostWindow, inspector) {
  let breadcrumbs = inspector.panelDoc.getElementById("inspector-breadcrumbs");
  let startBtn = breadcrumbs.querySelector(".scrollbutton-up");
  let endBtn = breadcrumbs.querySelector(".scrollbutton-down");
  let container = breadcrumbs.querySelector(".html-arrowscrollbox-inner");
  let breadcrumbsUpdated = inspector.once("breadcrumbs-updated");

  info("Selecting initial node");
  yield selectNode(NODE_SEVEN, inspector);

  // So just need to wait for a duration
  yield breadcrumbsUpdated;
  let initialCrumb = container.querySelector("button[checked]");
  is(isElementInViewport(hostWindow, initialCrumb), true,
     "initial element was visible");

  for (let node of NODES) {
    info("Checking for visibility of crumb " + node.title);
    if (node.action === "end") {
      info("Simulating click of end button");
      EventUtils.synthesizeMouseAtCenter(endBtn, {}, inspector.panelWin);
    } else if (node.action === "start") {
      info("Simulating click of start button");
      EventUtils.synthesizeMouseAtCenter(startBtn, {}, inspector.panelWin);
    }

    yield breadcrumbsUpdated;
    let selector = "button[title=\"" + node.title + "\"]";
    let relevantCrumb = container.querySelector(selector);
    is(isElementInViewport(hostWindow, relevantCrumb), true,
       node.title + " crumb is visible");
  }
}

function isElementInViewport(window, el) {
  let rect = el.getBoundingClientRect();

  return (
    rect.top >= 0 &&
    rect.left >= 0 &&
    rect.bottom <= window.innerHeight &&
    rect.right <= window.innerWidth
  );
}

registerCleanupFunction(function () {
  // Restore the host type for other tests.
  Services.prefs.clearUserPref("devtools.toolbox.host");
});
