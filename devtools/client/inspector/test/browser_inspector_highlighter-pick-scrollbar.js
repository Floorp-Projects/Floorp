/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that clicking on a scrollbar with the pick enabled does select the element that
// the scrollbar belongs to (see bug 1521712)

const TEST_URL = `data:text/html;charset=utf-8,
  <div id="wrapper" style="width:300px;height:300px;overflow:scroll">
    <div style="height:1000px;"></div>
  </div>`;
const TEST_NODE = "#wrapper";

add_task(async function() {
  const { toolbox, inspector } = await openInspectorForURL(TEST_URL);

  info("Start picking");
  await startPicker(toolbox);

  info("Click on a scrollbar in the document and expect the test node to be selected");
  const onSelect = inspector.once("inspector-updated");

  const scrollbarWidth = await getScrollbarWidth(TEST_NODE);
  const width = await getNodeWidth(TEST_NODE);
  await BrowserTestUtils.synthesizeMouse(TEST_NODE,
    width - scrollbarWidth / 2, 150, {}, gBrowser.selectedBrowser);

  await onSelect;

  is(inspector.selection.nodeFront.id, "wrapper", "The wrapper element was selected");
});

async function getScrollbarWidth(selector) {
  return ContentTask.spawn(gBrowser.selectedBrowser, selector, s => {
    const node = content.document.querySelector(s);
    return node.offsetWidth - node.clientWidth;
  });
}

async function getNodeWidth(selector) {
  return ContentTask.spawn(gBrowser.selectedBrowser, selector, s => {
    const node = content.document.querySelector(s);
    const rect = node.getBoundingClientRect();
    return rect.width;
  });
}
