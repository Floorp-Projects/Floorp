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
  const { inspector, testActor } = await openInspectorForURL(TEST_URI);

  const nodeFront = await getNodeFrontInFrame("#in-frame", "iframe", inspector);
  await selectNode(nodeFront, inspector);

  const markupLoaded = inspector.once("markuploaded");

  info("Reloading page.");
  await testActor.eval("location.reload()");

  info("Waiting for markupview to load after reload.");
  await markupLoaded;

  const reloadedNodeFront = await getNodeFrontInFrame(
    "#in-frame",
    "iframe",
    inspector
  );

  is(
    inspector.selection.nodeFront,
    reloadedNodeFront,
    "#in-frame selected after reload."
  );
});
