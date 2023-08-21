/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Tests for inspecting iframes and frames in browser context menu
const IFRAME_URI = `data:text/html;charset=utf-8,${encodeURI(
  `<div id="in-iframe">div in the iframe</div>`
)}`;
const TEST_IFRAME_DOC_URI = `data:text/html;charset=utf-8,${encodeURI(`
  <div id="salutation">Salution in top document</div>
  <iframe src="${IFRAME_URI}"></iframe>`)}`;

// <frameset> acts as the body element, so we can't use them in a document with other elements
// and have to set a dedicated document so we can test them.
const SAME_ORIGIN_FRAME_URI = `https://example.com/document-builder.sjs?html=<h2 id=in-same-origin-frame>h2 in the same origin frame</h2>`;
const REMOTE_ORIGIN_FRAME_URI = `https://example.org/document-builder.sjs?html=<h3 id=in-remote-frame>h3 in the remote frame</h3>`;
const TEST_FRAME_DOC_URI = `https://example.com/document-builder.sjs?html=${encodeURI(`
  <frameset cols="50%,50%">
    <frame class=same-origin src="${SAME_ORIGIN_FRAME_URI}"></frame>
    <frame class=remote src="${REMOTE_ORIGIN_FRAME_URI}"></frame>
  </frameset>`)}`;

add_task(async function () {
  await pushPref("devtools.command-button-frames.enabled", true);
  await addTab(TEST_IFRAME_DOC_URI);
  info(
    "Test inspecting element in <iframe> with top document selected in the frame picker"
  );
  await testContextMenuWithinFrame({
    selector: ["iframe", "#in-iframe"],
    nodeFrontGetter: inspector =>
      getNodeFrontInFrames(["iframe", "#in-iframe"], inspector),
  });

  info(
    "Test inspecting element in <iframe> with iframe document selected in the frame picker"
  );
  await changeToolboxToFrame(IFRAME_URI, 2);
  await testContextMenuWithinFrame({
    selector: ["iframe", "#in-iframe"],
    nodeFrontGetter: inspector => getNodeFront("#in-iframe", inspector),
  });
  await changeToolboxToFrame(TEST_IFRAME_DOC_URI, 2);

  await navigateTo(TEST_FRAME_DOC_URI);

  info(
    "Test inspecting element in same origin <frame> with top document selected in the frame picker"
  );
  await testContextMenuWithinFrame({
    selector: ["frame.same-origin", "#in-same-origin-frame"],
    nodeFrontGetter: inspector =>
      getNodeFrontInFrames(
        ["frame.same-origin", "#in-same-origin-frame"],
        inspector
      ),
  });

  info(
    "Test inspecting element in remote <frame> with top document selected in the frame picker"
  );
  await testContextMenuWithinFrame({
    selector: ["frame.remote", "#in-remote-frame"],
    nodeFrontGetter: inspector =>
      getNodeFrontInFrames(["frame.remote", "#in-remote-frame"], inspector),
  });

  info(
    "Test inspecting element in <frame> with frame document selected in the frame picker"
  );
  await changeToolboxToFrame(SAME_ORIGIN_FRAME_URI, 3);
  await testContextMenuWithinFrame({
    selector: ["frame.same-origin", "#in-same-origin-frame"],
    nodeFrontGetter: inspector =>
      getNodeFront("#in-same-origin-frame", inspector),
  });
});

/**
 * Pick a given element on the page with the 'Inspect Element' context menu entry and check
 * that the expected node is selected in the markup view.
 *
 * @param {Object} options
 * @param {Array<String>} options.selector: The selector of the element in the frame we
 *                        want to select
 * @param {Function} options.nodeFrontGetter: A function that will be executed to retrieve
 *                   the nodeFront that should be selected as a result of the 'Inspect Element' action.
 */
async function testContextMenuWithinFrame({ selector, nodeFrontGetter }) {
  info(
    `Opening inspector via 'Inspect Element' context menu on ${JSON.stringify(
      selector
    )}`
  );
  await clickOnInspectMenuItem(selector);

  info("Checking inspector state.");
  const inspector = getActiveInspector();
  const nodeFront = await nodeFrontGetter(inspector);

  is(
    inspector.selection.nodeFront,
    nodeFront,
    "Right node is selected in the markup view"
  );
}

/**
 * Select a specific document in the toolbox frame picker
 *
 * @param {String} frameUrl: The frame URL to select
 * @param {Number} expectedFramesCount: The number of frames that should be displayed in
 *                 the frame picker
 */
async function changeToolboxToFrame(frameUrl, expectedFramesCount) {
  const { toolbox } = getActiveInspector();

  const btn = toolbox.doc.getElementById("command-button-frames");
  const panel = toolbox.doc.getElementById("command-button-frames-panel");
  btn.click();
  ok(panel, "popup panel has created.");
  await waitUntil(() => panel.classList.contains("tooltip-visible"));

  info("Select the iframe in the frame list.");
  const menuList = toolbox.doc.getElementById("toolbox-frame-menu");
  const frames = Array.from(menuList.querySelectorAll(".command"));
  is(frames.length, expectedFramesCount, "Two frames shown in the switcher");

  const innerFrameButton = frames.find(
    frame => frame.querySelector(".label").textContent === frameUrl
  );
  ok(innerFrameButton, `Found frame button for inner frame "${frameUrl}"`);

  const newRoot = toolbox.getPanel("inspector").once("new-root");
  info(`Switch toolbox to inner frame "${frameUrl}"`);
  innerFrameButton.click();
  await newRoot;
}
