/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that canceling the element picker zooms back on the focused element. Bug 1224304.

const TEST_URL = URL_ROOT + "doc_inspector_long-divs.html";

add_task(function* () {
  let {inspector, toolbox, testActor} = yield openInspectorForURL(TEST_URL);

  yield selectAndHighlightNode("#focus-here", inspector);
  ok((yield testActor.assertHighlightedNode("#focus-here")),
     "The highlighter focuses on div#focus-here");
  ok(isSelectedMarkupNodeInView(),
     "The currently selected node is on the screen.");

  // Start the picker but skip focusing manually focusing on the target, let the element
  // picker do the focusing.
  yield startPicker(toolbox, true);
  yield moveMouseOver("#zoom-here");
  ok(!isSelectedMarkupNodeInView(),
     "The currently selected node is off the screen.");

  yield cancelPickerByShortcut();
  ok(isSelectedMarkupNodeInView(),
     "The currently selected node is focused back on the screen.");

  function cancelPickerByShortcut() {
    info("Key pressed. Waiting for picker to be canceled.");
    testActor.synthesizeKey({key: "VK_ESCAPE", options: {}});
    return inspector.toolbox.once("picker-canceled");
  }

  function moveMouseOver(selector) {
    info(`Waiting for element ${selector} to be hovered in the markup view`);
    testActor.synthesizeMouse({
      options: {type: "mousemove"},
      center: true,
      selector: selector
    });
    return inspector.markup.once("showcontainerhovered");
  }

  function isSelectedMarkupNodeInView() {
    const selectedNodeContainer = inspector.markup._selectedContainer.elt;
    const bounds = selectedNodeContainer.getBoundingClientRect();
    return bounds.top > 0 && bounds.bottom > 0;
  }
});
