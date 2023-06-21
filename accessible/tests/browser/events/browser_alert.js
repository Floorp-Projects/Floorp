/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that alert events aren't fired when reflow happens but no actual
 * insertion occurs.
 */
addAccessibleTask(
  `
<div id="alert" role="alert">
  <div id="content" hidden>content</div>
</div>
  `,
  async function (browser, docAcc) {
    const alert = findAccessibleChildByID(docAcc, "alert");
    info("Showing content");
    await contentSpawnMutation(
      browser,
      { expected: [[EVENT_ALERT, alert]] },
      () => {
        content.document.getElementById("content").hidden = false;
      }
    );
    info("Changing content display style and removing text");
    const content = findAccessibleChildByID(docAcc, "content");
    await contentSpawnMutation(
      browser,
      {
        expected: [[EVENT_REORDER, content]],
        unexpected: [[EVENT_ALERT, alert]],
      },
      () => {
        const node = content.document.getElementById("content");
        node.textContent = "";
        // This causes the node's layout frame to be reconstructed. This in
        // turn causes a11y to queue it as an insertion in case there were
        // changes. Because it already has an Accessible, This node is skipped
        // when processing insertions, so we should not fire an alert event.
        node.style.display = "flex";
      }
    );
  },
  { chrome: true, topLevel: true, remoteIframe: true }
);
