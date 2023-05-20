/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the node picker continues to work after page reload

const TEST_URL = URL_ROOT_SSL + "doc_inspector_highlighter_dom.html";

add_task(async function () {
  const { inspector, toolbox } = await openInspectorForURL(TEST_URL);

  await startPicker(toolbox);

  info("Selecting the simple-div1 DIV");
  await hoverElement(inspector, "#simple-div1");

  // Reload the current page (navigate to the same URL)
  await navigateTo(TEST_URL);

  // hoverElement() resolves after both the "picker-node-hovered" event
  // and the "highlighter-shown" event are triggered. If this test doesn't timeout,
  // it means node picking and node highlighting continue to work as expected.
  info("Selecting the simple-div2 DIV after reload");
  await hoverElement(inspector, "#simple-div2");

  info("Picking the simple-div2 DIV after reload");
  await pickElement(inspector, "#simple-div2", 0, 0);

  is(
    inspector.selection.nodeFront.id,
    "simple-div2",
    "The simple-div2 DIV has been picked after reload"
  );
});
