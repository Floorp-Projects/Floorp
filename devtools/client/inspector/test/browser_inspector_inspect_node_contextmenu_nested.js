/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test Inspect Element feature with nested iframes of different origins.
// root (example.net)
//   remote_frame1 (example.com)
//     nested_same_process_frame (example.com, same as parent)
//   same_process_frame (example.net, same as root)
//   remote_frame2 (example.org)
//     nested_remote_frame (example.com)

// Build the remote_frame1 hierarchy
const NESTED_SAME_PROCESS_FRAME_URI =
  "https://example.com/document-builder.sjs?html=" +
  encodeURI(
    `<div id="in-nested_same_process_frame">in-nested_same_process_frame`
  );
const REMOTE_FRAME1_HTML = `<iframe id="nested_same_process_frame" src="${NESTED_SAME_PROCESS_FRAME_URI}"></iframe>`;
const REMOTE_FRAME1_URI =
  "https://example.com/document-builder.sjs?html=" +
  encodeURI(REMOTE_FRAME1_HTML);

// Build the same_process_frame hierarchy
const SAME_PROCESS_FRAME_URI =
  "https://example.net/document-builder.sjs?html=" +
  encodeURI(`<div id="in-same_process_frame">in-same_process_frame`);

// Build the remote_frame2 hierarchy
const NESTED_REMOTE_FRAME_URI =
  "https://example.com/document-builder.sjs?html=" +
  encodeURI(`<div id="in-nested_remote_frame">in-nested_remote_frame`);
const REMOTE_FRAME2_HTML = `<iframe id="nested_remote_frame" src="${NESTED_REMOTE_FRAME_URI}"></iframe>`;
const REMOTE_FRAME2_URI =
  "https://example.org/document-builder.sjs?html=" +
  encodeURI(REMOTE_FRAME2_HTML);

// Assemble all frames in a single test page.
const HTML = `
  <iframe id="remote_frame1" src="${REMOTE_FRAME1_URI}"></iframe>
  <iframe id="same_process_frame" src="${SAME_PROCESS_FRAME_URI}"></iframe>
  <iframe id="remote_frame2" src="${REMOTE_FRAME2_URI}"></iframe>
`;
const TEST_URI =
  "https://example.net/document-builder.sjs?html=" + encodeURI(HTML);

add_task(async function () {
  const tab = await addTab(TEST_URI);

  info("Retrieve the browsing context for nested_same_process_frame");
  const nestedSameProcessFrameBC = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    function () {
      const remote_frame1 = content.document.getElementById("remote_frame1");
      return SpecialPowers.spawn(remote_frame1, [], function () {
        return content.document.getElementById(
          "nested_same_process_frame"
        ).browsingContext;
      });
    }
  );
  await inspectElementInBrowsingContext(
    nestedSameProcessFrameBC,
    "#in-nested_same_process_frame"
  );
  checkSelectedNode("in-nested_same_process_frame");

  info("Retrieve the browsing context for same_process_frame");
  const sameProcessFrameBC = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    function () {
      return content.document.getElementById("same_process_frame")
        .browsingContext;
    }
  );
  await inspectElementInBrowsingContext(
    sameProcessFrameBC,
    "#in-same_process_frame"
  );
  checkSelectedNode("in-same_process_frame");

  info("Retrieve the browsing context for nested_remote_frame");
  const nestedRemoteFrameBC = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    function () {
      const remote_frame2 = content.document.getElementById("remote_frame2");
      return SpecialPowers.spawn(remote_frame2, [], function () {
        return content.document.getElementById(
          "nested_remote_frame"
        ).browsingContext;
      });
    }
  );
  await inspectElementInBrowsingContext(
    nestedRemoteFrameBC,
    "#in-nested_remote_frame"
  );
  checkSelectedNode("in-nested_remote_frame");
});

/**
 * Check the id of currently selected node front in the inspector.
 */
function checkSelectedNode(id) {
  const inspector = getActiveInspector();
  is(
    inspector.selection.nodeFront.id,
    id,
    "The correct node is selected in the markup view"
  );
}

/**
 * Use inspect element on the element matching the provided selector in a given
 * browsing context.
 *
 * Note: adapted from head.js `clickOnInspectMenuItem` in order to work with a
 * browsingContext instead of a test actor.
 */
async function inspectElementInBrowsingContext(browsingContext, selector) {
  info(
    `Show the context menu for selector "${selector}" in browsing context ${browsingContext.id}`
  );
  const contentAreaContextMenu = document.querySelector(
    "#contentAreaContextMenu"
  );
  const contextOpened = once(contentAreaContextMenu, "popupshown");

  const options = { type: "contextmenu", button: 2 };
  await BrowserTestUtils.synthesizeMouse(
    selector,
    0,
    0,
    options,
    browsingContext
  );

  await contextOpened;

  info("Triggering the inspect action");
  await gContextMenu.inspectNode();

  info("Hiding the menu");
  const contextClosed = once(contentAreaContextMenu, "popuphidden");
  contentAreaContextMenu.hidePopup();
  await contextClosed;
}
