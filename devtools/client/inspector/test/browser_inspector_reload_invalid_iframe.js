/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// When reselecting the selected node, if an element expected to be an iframe is
// NOT actually an iframe, we should still fallback on the root body element.

const TEST_URI = "data:text/html;charset=utf-8,<div id='fake-iframe'>";

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URI);

  info("Replace fake-iframe div with a real iframe");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function () {
    await new Promise(resolve => {
      // Remove the fake-iframe div
      content.document.querySelector("#fake-iframe").remove();

      // Create an iframe element with the same id "fake-iframe".
      const iframe = content.document.createElement("iframe");
      content.document.body.appendChild(iframe);
      iframe.setAttribute("id", "fake-iframe");

      iframe.contentWindow.addEventListener("load", () => {
        // Create a div element and append it to the iframe
        const div = content.document.createElement("div");
        div.id = "in-frame";
        div.textContent = "div in frame";

        const frameContent =
          iframe.contentWindow.document.querySelector("body");
        frameContent.appendChild(div);
        resolve();
      });
    });
  });

  ok(
    await hasMatchingElementInContentPage("iframe"),
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
