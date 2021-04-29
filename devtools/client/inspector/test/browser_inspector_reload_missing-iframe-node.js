/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check that the markup view selection falls back to the document body if an iframe node
// becomes missing after a reload. This can happen if the iframe and its contents are
// added dynamically to the page before reloading.

const TEST_URI = "data:text/html;charset=utf-8,";

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(TEST_URI);

  info("Create new iframe and add it to the page.");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    await new Promise(resolve => {
      const iframe = content.document.createElement("iframe");
      content.document.body.appendChild(iframe);

      iframe.contentWindow.addEventListener("load", () => {
        // Create a div element and append it to the iframe
        const div = content.document.createElement("div");
        div.id = "in-frame";
        div.textContent = "div in frame";

        const frameContent = iframe.contentWindow.document.querySelector(
          "body"
        );
        frameContent.appendChild(div);
        resolve();
      });
    });
  });
  ok(
    await testActor.hasNode("iframe"),
    "The iframe has been added to the page"
  );

  info("Select node inside iframe.");
  await selectNodeInFrames(["iframe", "#in-frame"], inspector);

  const markupLoaded = inspector.once("markuploaded");

  info("Reloading page.");
  await navigateTo(TEST_URI);

  info("Waiting for markupview to load after reload.");
  await markupLoaded;

  const rootNodeFront = await getNodeFront("body", inspector);

  is(
    inspector.selection.nodeFront,
    rootNodeFront,
    "body node selected after reload."
  );
});
