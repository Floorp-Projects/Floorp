/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Check that the markup view selection is preserved even if the selection is
// in a nested iframe.

const NESTED_IFRAME_URL = `https://example.com/document-builder.sjs?html=${encodeURIComponent(
  "<h3>second level iframe</h3>"
)}&delay=500`;

const TEST_URI = `data:text/html;charset=utf-8,
  <h1>Top-level</h1>
  <iframe id=first-level
    src='data:text/html;charset=utf-8,${encodeURIComponent(
      `<h2>first level iframe</h2><iframe id=second-level src="${NESTED_IFRAME_URL}"></iframe>`
    )}'
  ></iframe>`;

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URI);

  await selectNodeInFrames(
    ["iframe#first-level", "iframe#second-level", "h3"],
    inspector
  );

  const markupLoaded = inspector.once("markuploaded");

  info("Reloading page.");
  await navigateTo(TEST_URI);

  info("Waiting for markupview to load after reload.");
  await markupLoaded;

  // This was broken at some point, see Bug 1733539.
  ok(true, "The markup did reload fine");

  const reloadedNodeFront = await getNodeFrontInFrames(
    ["iframe#first-level", "iframe#second-level", "h3"],
    inspector
  );

  is(
    inspector.selection.nodeFront,
    reloadedNodeFront,
    `h3 selected after reload`
  );
});
