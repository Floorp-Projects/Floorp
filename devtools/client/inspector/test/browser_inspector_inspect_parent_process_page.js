/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the "inspect element" context menu item works for parent process
// chrome pages.
add_task(async function () {
  const tab = await addTab("about:debugging");

  const browser = tab.linkedBrowser;
  const document = browser.contentDocument;

  info("Wait until Connect page is displayed");
  await waitUntil(() => document.querySelector(".qa-connect-page"));
  const inspector = await clickOnInspectMenuItem(".qa-network-form-input");

  const selectedNode = inspector.selection.nodeFront;
  ok(selectedNode, "A node is selected in the inspector");
  is(
    selectedNode.tagName.toLowerCase(),
    "input",
    "The selected node has the correct tagName"
  );
  ok(
    selectedNode.className.includes("qa-network-form-input"),
    "The selected node has the expected className"
  );
});
