/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,
  <!DOCTYPE html>
  <video controls></video>`;

// Test that using the node picker on user agent widgets only selects shadow dom
// elements if `devtools.inspector.showAllAnonymousContent` is true.
// If not, we should only surface the host, in this case, the <video>.
//
// For this test we use a <video controls> tag, which is using shadow dom.
add_task(async function () {
  // Run the test for both values for devtools.inspector.showAllAnonymousContent
  await runUserAgentWidgetPickerTest({ enableAnonymousContent: false });
  await runUserAgentWidgetPickerTest({ enableAnonymousContent: true });
});

async function runUserAgentWidgetPickerTest({ enableAnonymousContent }) {
  await pushPref(
    "devtools.inspector.showAllAnonymousContent",
    enableAnonymousContent
  );
  const { inspector, toolbox } = await openInspectorForURL(TEST_URI);

  info("Use the node picker inside the <video> element");
  await startPicker(toolbox);
  const onPickerStopped = toolbox.nodePicker.once("picker-stopped");
  await pickElement(inspector, "video", 5, 5);
  await onPickerStopped;

  const selectedNode = inspector.selection.nodeFront;
  if (enableAnonymousContent) {
    // We do not assert specifically which node was selected, we just want to
    // check the node was under the shadow DOM for the <video type=date>
    const shadowHost = getShadowHost(selectedNode);
    ok(
      selectedNode.tagName.toLowerCase() !== "video",
      "The selected node is not the <video>"
    );
    ok(shadowHost, "The selected node is in a shadow root");
    is(shadowHost.tagName.toLowerCase(), "video", "The shadowHost is <video>");
  } else {
    is(
      selectedNode.tagName.toLowerCase(),
      "video",
      "The selected node is the <video>"
    );
  }
}

/**
 * Retrieve the nodeFront for the shadow host containing the provided nodeFront.
 * Returns null if the nodeFront is not in a shadow DOM.
 *
 * @param {NodeFront} nodeFront
 *        The nodeFront for which we want to retrieve the shadow host.
 * @return {NodeFront} The nodeFront corresponding to the shadow host, or null
 *         if the nodeFront is not in shadow DOM.
 */
function getShadowHost(nodeFront) {
  let parent = nodeFront;
  while (parent) {
    if (parent.isShadowHost) {
      return parent;
    }
    parent = parent.parentOrHost();
  }
  return null;
}
