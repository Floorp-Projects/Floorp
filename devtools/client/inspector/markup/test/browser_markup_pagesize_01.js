/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the markup view loads only as many nodes as specified by the
// devtools.markup.pagesize preference.

Services.prefs.setIntPref("devtools.markup.pagesize", 5);

const TEST_URL = URL_ROOT + "doc_markup_pagesize_01.html";
const TEST_DATA = [{
  desc: "Select the last item",
  selector: "#z",
  expected: "*more*vwxyz"
}, {
  desc: "Select the first item",
  selector: "#a",
  expected: "abcde*more*"
}, {
  desc: "Select the last item",
  selector: "#z",
  expected: "*more*vwxyz"
}, {
  desc: "Select an already-visible item",
  selector: "#v",
  // Because "v" was already visible, we shouldn't have loaded
  // a different page.
  expected: "*more*vwxyz"
}, {
  desc: "Verify childrenDirty reloads the page",
  selector: "#w",
  forceReload: true,
  // But now that we don't already have a loaded page, selecting
  // w should center around w.
  expected: "*more*uvwxy*more*"
}];

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  info("Start iterating through the test data");
  for (const step of TEST_DATA) {
    info("Start test: " + step.desc);

    if (step.forceReload) {
      await forceReload(inspector);
    }
    info("Selecting the node that corresponds to " + step.selector);
    await selectNode(step.selector, inspector);

    info("Checking that the right nodes are shwon");
    await assertChildren(step.expected, inspector);
  }

  info("Checking that clicking the more button loads everything");
  await clickShowMoreNodes(inspector);
  await inspector.markup._waitForChildren();
  await assertChildren("abcdefghijklmnopqrstuvwxyz", inspector);
});

async function assertChildren(expected, inspector) {
  const container = await getContainerForSelector("body", inspector);
  let found = "";
  for (const child of container.children.children) {
    if (child.classList.contains("more-nodes")) {
      found += "*more*";
    } else {
      found += child.container.node.getAttribute("id");
    }
  }
  is(found, expected, "Got the expected children.");
}

async function forceReload(inspector) {
  const container = await getContainerForSelector("body", inspector);
  container.childrenDirty = true;
}

async function clickShowMoreNodes(inspector) {
  const container = await getContainerForSelector("body", inspector);
  const button = container.elt.querySelector("button");
  const win = button.ownerDocument.defaultView;
  EventUtils.sendMouseEvent({type: "click"}, button, win);
}
