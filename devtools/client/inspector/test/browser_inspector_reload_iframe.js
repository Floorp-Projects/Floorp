/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Check that the markup view selection is preserved even if the selection is
// in an iframe.

const FRAME_URI =
  "data:text/html;charset=utf-8," +
  encodeURI(`<div id="in-frame">div in the iframe</div>`);
const HTML = `
  <iframe src="${FRAME_URI}"></iframe>
`;

const TEST_URI = "data:text/html;charset=utf-8," + encodeURI(HTML);

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URI);

  await selectNodeInFrames(["iframe", "#in-frame"], inspector);

  const markupLoaded = inspector.once("markuploaded");

  info("Reloading page.");
  await navigateTo(TEST_URI);

  info("Waiting for markupview to load after reload.");
  await markupLoaded;

  const reloadedNodeFront = await getNodeFrontInFrames(
    ["iframe", "#in-frame"],
    inspector
  );

  is(
    inspector.selection.nodeFront,
    reloadedNodeFront,
    "#in-frame selected after reload."
  );
});
