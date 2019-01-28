/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the scrollable badge is shown next to scrollable elements, and is updated
// dynamically when necessary.

const TEST_URI = `
  <style type="text/css">
    #wrapper {
      width: 300px;
      height: 300px;
      overflow: scroll;
    }
    #wrapper.no-scroll {
      overflow: hidden;
    }
    #content {
      height: 1000px;
    }
  </style>
  <div id="wrapper">
    <div id="content"></div>
  </div>
`;

add_task(async function() {
  info("Enable the scrollable badge feature");
  await pushPref("devtools.inspector.scrollable-badges.enabled", true);

  const { inspector } = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  let badge = await getBadgeEl(inspector);
  ok(badge, "The scrollable badge exists on the test node");

  info("Make the test node non-scrollable");
  let onStateChanged = inspector.walker.once("scrollable-change");
  await toggleScrollableClass();
  await onStateChanged;

  badge = await getBadgeEl(inspector);
  ok(!badge, "The scrollable badge doesn't exist anymore");

  info("Make the test node scrollable again");
  onStateChanged = inspector.walker.once("scrollable-change");
  await toggleScrollableClass();
  await onStateChanged;

  badge = await getBadgeEl(inspector);
  ok(badge, "The scrollable badge exists again");
});

async function getBadgeEl(inspector) {
  const wrapperMarkupContainer = await getContainerForSelector("#wrapper", inspector);
  return wrapperMarkupContainer.elt.querySelector(".inspector-badge.scrollable-badge");
}

async function toggleScrollableClass() {
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    content.document.querySelector("#wrapper").classList.toggle("no-scroll");
  });
}
