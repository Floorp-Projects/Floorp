/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the start and end buttons on the breadcrumb trail bring the right
// crumbs into the visible area, for both LTR and RTL

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm");
PromiseTestUtils.whitelistRejectionsGlobally(/Connection closed/);

const { Toolbox } = require("devtools/client/framework/toolbox");

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
  { action: "end", title: NODE_SIX },
];

add_task(async function() {
  const { inspector, toolbox } = await openInspectorForURL(TEST_URI);

  // No way to wait for scrolling to end (Bug 1172171)
  // Rather than wait a max time; limit test to instant scroll behavior
  inspector.breadcrumbs.arrowScrollBox.scrollBehavior = "instant";

  await toolbox.switchHost(Toolbox.HostType.WINDOW);
  const hostWindow = toolbox.win.parent;
  const originalWidth = hostWindow.outerWidth;
  const originalHeight = hostWindow.outerHeight;
  const inspectorResized = inspector.once("inspector-resize");
  hostWindow.resizeTo(640, 300);
  await inspectorResized;

  info("Testing transitions ltr");
  await pushPref("intl.uidirection", 0);
  await testBreadcrumbTransitions(hostWindow, inspector);

  info("Testing transitions rtl");
  await pushPref("intl.uidirection", 1);
  await testBreadcrumbTransitions(hostWindow, inspector);

  hostWindow.resizeTo(originalWidth, originalHeight);
});

async function testBreadcrumbTransitions(hostWindow, inspector) {
  const breadcrumbs = inspector.panelDoc.getElementById("inspector-breadcrumbs");
  const startBtn = breadcrumbs.querySelector(".scrollbutton-up");
  const endBtn = breadcrumbs.querySelector(".scrollbutton-down");
  const container = breadcrumbs.querySelector(".html-arrowscrollbox-inner");
  const breadcrumbsUpdated = inspector.once("breadcrumbs-updated");

  info("Selecting initial node");
  await selectNode(NODE_SEVEN, inspector);

  // So just need to wait for a duration
  await breadcrumbsUpdated;
  const initialCrumb = container.querySelector("button[checked]");
  is(isElementInViewport(hostWindow, initialCrumb), true,
     "initial element was visible");

  for (const node of NODES) {
    info("Checking for visibility of crumb " + node.title);
    if (node.action === "end") {
      info("Simulating click of end button");
      EventUtils.synthesizeMouseAtCenter(endBtn, {}, inspector.panelWin);
    } else if (node.action === "start") {
      info("Simulating click of start button");
      EventUtils.synthesizeMouseAtCenter(startBtn, {}, inspector.panelWin);
    }

    await breadcrumbsUpdated;
    const selector = "button[title=\"" + node.title + "\"]";
    const relevantCrumb = container.querySelector(selector);
    is(isElementInViewport(hostWindow, relevantCrumb), true,
       node.title + " crumb is visible");
  }
}

function isElementInViewport(window, el) {
  const rect = el.getBoundingClientRect();

  return (
    rect.top >= 0 &&
    rect.left >= 0 &&
    rect.bottom <= window.innerHeight &&
    rect.right <= window.innerWidth
  );
}

registerCleanupFunction(function() {
  // Restore the host type for other tests.
  Services.prefs.clearUserPref("devtools.toolbox.host");
});
