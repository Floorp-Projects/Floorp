/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the geometry highlighter elements on remote frame.

const TEST_URL = `data:text/html;charset=utf-8,
  <iframe src="https://example.org/document-builder.sjs?html=${encodeURIComponent(`
    <div id='positioned' style='
      background:yellow;
      position:absolute;
      left:5rem;
      top:30px;'
    >
      Hello from iframe
    </div>`)}">
  </iframe>`;

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);

  info("Select the absolute positioned node in the iframe");
  await selectNodeInFrames(["iframe", "#positioned"], inspector);

  info("Click on the button enabling the highlighter");
  const onHighlighterShown = inspector.highlighters.once(
    "geometry-editor-highlighter-shown"
  );
  const boxModelPanel = inspector.getPanel("boxmodel");
  const button = await waitFor(() =>
    boxModelPanel.document.querySelector(".layout-geometry-editor")
  );
  button.click();

  await onHighlighterShown;
  ok(true, "Highlighter is displayed");

  info("Click on the button again to disable the highlighter");
  const onHighlighterHidden = inspector.highlighters.once(
    "geometry-editor-highlighter-hidden"
  );

  button.click();
  await onHighlighterHidden;
  ok("Highlighter is hidden");
});
