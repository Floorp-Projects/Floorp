/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals getTestActorWithoutToolbox */

"use strict";

// Tests for inspect node in browser context menu

const FRAME_URI =
  "data:text/html;charset=utf-8," +
  encodeURI(`<div id="in-frame">div in the iframe</div>`);
const HTML = `
  <div id="salutation">Salution in top document</div>
  <iframe src="${FRAME_URI}"></iframe>
`;

const TEST_URI = "data:text/html;charset=utf-8," + encodeURI(HTML);

add_task(async function() {
  await pushPref("devtools.command-button-frames.enabled", true);

  const tab = await addTab(TEST_URI);
  const testActor = await getTestActorWithoutToolbox(tab);

  // Use context menu with root frame selected in toolbox
  await testContextMenuWithinIframe(testActor, async inspector => {
    return getNodeFrontInFrames(["iframe", "#in-frame"], inspector);
  });

  // Use context menu with inner frame selected in toolbox
  await changeToolboxToInnerFrame();
  await testContextMenuWithinIframe(testActor, async inspector => {
    return getNodeFront("#in-frame", inspector);
  });
});

async function testContextMenuWithinIframe(testActor, nodeFrontGetter) {
  info(
    "Opening inspector via 'Inspect Element' context menu item within an iframe"
  );
  const selector = ["iframe", "#in-frame"];
  await clickOnInspectMenuItem(testActor, selector);

  info("Checking inspector state.");
  const inspector = await getActiveInspector();
  const nodeFront = await nodeFrontGetter(inspector);

  is(
    inspector.selection.nodeFront,
    nodeFront,
    "Right node is selected in the markup view"
  );
}

async function changeToolboxToInnerFrame() {
  const { toolbox } = await getActiveInspector();

  const btn = toolbox.doc.getElementById("command-button-frames");
  const panel = toolbox.doc.getElementById("command-button-frames-panel");
  btn.click();
  ok(panel, "popup panel has created.");
  await waitUntil(() => panel.classList.contains("tooltip-visible"));

  info("Select the iframe in the frame list.");
  const menuList = toolbox.doc.getElementById("toolbox-frame-menu");
  const frames = Array.from(menuList.querySelectorAll(".command"));
  is(frames.length, 2, "Two frames shown in the switcher");

  const innerFrameButton = frames.filter(
    b => b.querySelector(".label").textContent === FRAME_URI
  )[0];
  ok(innerFrameButton, "Found frame button for inner frame");

  const newRoot = toolbox.getPanel("inspector").once("new-root");
  info("Switch toolbox to inner frame");
  innerFrameButton.click();
  await newRoot;
}
