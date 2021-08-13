/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that using the iframe picker usage + bfcache navigations don't break the toolbox

const IFRAME_URL = `https://example.com/document-builder.sjs?html=<div id=in-iframe>frame</div>`;

const URL =
  "https://example.com/document-builder.sjs?html=" +
  `<iframe src='${IFRAME_URL}'></iframe><div id=top>top</div>`;

add_task(async function() {
  await pushPref("devtools.command-button-frames.enabled", true);

  // Don't show the third panel to limit the logs and activity.
  await pushPref("devtools.inspector.three-pane-enabled", false);
  await pushPref("devtools.inspector.activeSidebar", "ruleview");

  const { inspector, toolbox } = await openInspectorForURL(URL);

  info("Verify we are on the top level document");
  await assertMarkupViewAsTree(
    `
    body
      iframe!ignore-children
      div id="top"`,
    "body",
    inspector
  );

  info("Navigate to a different page to put the current page in the bfcache");
  let onMarkupLoaded = getOnInspectorReadyAfterNavigation(inspector);
  await navigateTo(
    "https://example.org/document-builder.sjs?html=example.org page"
  );
  await onMarkupLoaded;

  info("Navigate back to the example.com page");
  onMarkupLoaded = getOnInspectorReadyAfterNavigation(inspector);
  gBrowser.goBack();
  await onMarkupLoaded;

  // Verify that the frame map button is empty at the moment.
  const btn = toolbox.doc.getElementById("command-button-frames");
  ok(!btn.firstChild, "The frame list button doesn't have any children");

  // Open frame menu and wait till it's available on the screen.
  const panel = toolbox.doc.getElementById("command-button-frames-panel");
  btn.click();
  ok(panel, "popup panel has created.");
  await waitUntil(() => panel.classList.contains("tooltip-visible"));

  // Verify that the menu is populated.
  const menuList = toolbox.doc.getElementById("toolbox-frame-menu");
  const frames = Array.from(menuList.querySelectorAll(".command")).sort(
    (a, b) => a.textContent < b.textContent
  );
  is(frames.length, 2, "We have both frames in the menu");
  const [topLevelFrameItem, iframeItem] = frames;

  is(
    topLevelFrameItem.querySelector(".label").textContent,
    URL,
    "Got top-level document in the list"
  );
  is(
    iframeItem.querySelector(".label").textContent,
    IFRAME_URL,
    "Got iframe document in the list"
  );

  info("Select the iframe in the iframe picker");
  const newRoot = inspector.once("new-root");
  iframeItem.click();
  await newRoot;

  info("Check that the markup view was updated");
  await assertMarkupViewAsTree(
    `
    body
      div id="in-iframe"`,
    "body",
    inspector
  );

  info("Go forward (to example.org page)");
  onMarkupLoaded = getOnInspectorReadyAfterNavigation(inspector);
  gBrowser.goForward();
  await onMarkupLoaded;

  info("And go back again to example.com page");
  onMarkupLoaded = getOnInspectorReadyAfterNavigation(inspector);
  gBrowser.goBack();
  await onMarkupLoaded;

  info("Check that the document markup is displayed as expected");
  await assertMarkupViewAsTree(
    `
    body
      iframe!ignore-children
      div id="top"`,
    "body",
    inspector
  );
});

function getOnInspectorReadyAfterNavigation(inspector) {
  const promises = [inspector.once("reloaded")];

  if (isFissionEnabled() || isServerTargetSwitchingEnabled()) {
    // the inspector is initializing the accessibility front in onTargetAvailable, so we
    // need to wait for the target to be processed, otherwise we may end up with pending
    // promises failures.
    promises.push(
      inspector.toolbox.commands.targetCommand.once(
        "processed-available-target"
      )
    );
  }

  return Promise.all(promises);
}
