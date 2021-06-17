/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that canceling the element picker zooms back on the focused element. Bug 1224304.

const TEST_URL = URL_ROOT + "doc_inspector_long-divs.html";

add_task(async function() {
  const {
    inspector,
    toolbox,
    highlighterTestFront,
  } = await openInspectorForURL(TEST_URL);

  await selectAndHighlightNode("#focus-here", inspector);
  ok(
    await highlighterTestFront.assertHighlightedNode("#focus-here"),
    "The highlighter focuses on div#focus-here"
  );
  ok(
    isSelectedMarkupNodeInView(),
    "The currently selected node is on the screen."
  );

  await startPicker(toolbox);
  await hoverElement(inspector, "#zoom-here");
  ok(
    !isSelectedMarkupNodeInView(),
    "The currently selected node is off the screen."
  );

  await cancelPickerByShortcut();
  ok(
    isSelectedMarkupNodeInView(),
    "The currently selected node is focused back on the screen."
  );

  function cancelPickerByShortcut() {
    info("Key pressed. Waiting for picker to be canceled.");
    BrowserTestUtils.synthesizeKey("VK_ESCAPE", {}, gBrowser.selectedBrowser);
    return toolbox.nodePicker.once("picker-node-canceled");
  }

  function isSelectedMarkupNodeInView() {
    const selectedNodeContainer = inspector.markup._selectedContainer.elt;
    const bounds = selectedNodeContainer.getBoundingClientRect();
    return bounds.top > 0 && bounds.bottom > 0;
  }
});
