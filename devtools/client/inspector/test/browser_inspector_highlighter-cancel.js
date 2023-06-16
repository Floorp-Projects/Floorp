/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that canceling the element picker zooms back on the focused element. Bug 1224304.

const TEST_URL = URL_ROOT + "doc_inspector_long-divs.html";

const isMac = Services.appinfo.OS === "Darwin";
const TESTS = [
  {
    key: "VK_ESCAPE",
    options: {},
    // Escape would open the split console if the toolbox was focused
    focusToolbox: false,
  },
  {
    key: "C",
    options: {
      metaKey: isMac,
      ctrlKey: !isMac,
      shiftKey: true,
    },
    // Test with the toolbox focused to check the client side event listener.
    focusToolbox: true,
  },
  {
    key: "C",
    options: {
      metaKey: isMac,
      ctrlKey: !isMac,
      shiftKey: true,
    },
    // Test with the content page focused to check the actor's event listener.
    focusToolbox: false,
  },
];

for (const { key, options, focusToolbox } of TESTS) {
  add_task(async function () {
    info(`Testing cancel shortcut: ${key} with toolbox focus: ${focusToolbox}`);

    const { inspector, toolbox, highlighterTestFront } =
      await openInspectorForURL(TEST_URL);
    await selectAndHighlightNode("#focus-here", inspector);
    ok(
      await highlighterTestFront.assertHighlightedNode("#focus-here"),
      "The highlighter focuses on div#focus-here"
    );
    ok(
      isSelectedMarkupNodeInView(inspector),
      "The currently selected node is on the screen."
    );

    await startPicker(toolbox);

    await hoverElement(inspector, "#zoom-here");
    ok(
      !isSelectedMarkupNodeInView(inspector),
      "The currently selected node is off the screen."
    );

    if (focusToolbox) {
      toolbox.win.focus();
    }

    await cancelPickerByShortcut(toolbox, key, options);
    ok(
      isSelectedMarkupNodeInView(inspector),
      "The currently selected node is focused back on the screen."
    );

    is(
      await highlighterTestFront.isHighlighting(),
      false,
      "The highlighter was hidden"
    );
  });
}

async function cancelPickerByShortcut(toolbox, key, options) {
  info("Key pressed. Waiting for picker to be canceled.");
  const onStopped = toolbox.nodePicker.once("picker-node-canceled");
  EventUtils.synthesizeKey(key, options, toolbox.win);
  return onStopped;
}

function isSelectedMarkupNodeInView(inspector) {
  const selectedNodeContainer = inspector.markup._selectedContainer.elt;
  const bounds = selectedNodeContainer.getBoundingClientRect();
  return bounds.top > 0 && bounds.bottom > 0;
}
