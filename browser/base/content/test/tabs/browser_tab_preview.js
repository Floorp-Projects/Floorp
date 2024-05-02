/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

async function openPreview(tab) {
  const previewShown = BrowserTestUtils.waitForPopupEvent(
    document.getElementById("tab-preview-panel"),
    "shown"
  );
  EventUtils.synthesizeMouseAtCenter(tab, { type: "mouseover" });
  return previewShown;
}

async function closePreviews() {
  const tabs = document.getElementById("tabbrowser-tabs");
  const previewHidden = BrowserTestUtils.waitForPopupEvent(
    document.getElementById("tab-preview-panel"),
    "hidden"
  );
  EventUtils.synthesizeMouse(tabs, 0, tabs.outerHeight + 1, {
    type: "mouseout",
  });
  return previewHidden;
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.cardPreview.enabled", true],
      ["browser.tabs.cardPreview.showThumbnails", false],
      ["browser.tabs.tooltipsShowPidAndActiveness", false],
      ["ui.tooltip.delay_ms", 0],
    ],
  });
});

/**
 * Verify the following:
 *
 * 1. Tab preview card appears when the mouse hovers over a tab
 * 2. Tab preview card shows the correct preview for the tab being hovered
 * 3. Tab preview card is dismissed when the mouse leaves the tab bar
 */
add_task(async function hoverTests() {
  const tabUrl1 =
    "data:text/html,<html><head><title>First New Tab</title></head><body>Hello</body></html>";
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl1);
  const tabUrl2 =
    "data:text/html,<html><head><title>Second New Tab</title></head><body>Hello</body></html>";
  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl2);
  const previewContainer = document.getElementById("tab-preview-panel");

  await openPreview(tab1);
  Assert.equal(
    previewContainer.querySelector(".tab-preview-title").innerText,
    "First New Tab",
    "Preview of tab1 shows correct title"
  );

  await closePreviews();
  await openPreview(tab2);
  Assert.equal(
    previewContainer.querySelector(".tab-preview-title").innerText,
    "Second New Tab",
    "Preview of tab2 shows correct title"
  );

  await closePreviews();

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });
});

/**
 * Verify that the pid and activeness statuses are not shown
 * when the flag is not enabled.
 */
add_task(async function pidAndActivenessHiddenByDefaultTests() {
  const tabUrl1 =
    "data:text/html,<html><head><title>First New Tab</title></head><body>Hello</body></html>";
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl1);
  const previewContainer = document.getElementById("tab-preview-panel");

  await openPreview(tab1);
  Assert.equal(
    previewContainer.querySelector(".tab-preview-pid").innerText,
    "",
    "Tab PID is not shown"
  );
  Assert.equal(
    previewContainer.querySelector(".tab-preview-activeness").innerText,
    "",
    "Tab activeness is not shown"
  );

  await closePreviews();

  BrowserTestUtils.removeTab(tab1);

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });
});

add_task(async function pidAndActivenessTests() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.tooltipsShowPidAndActiveness", true]],
  });

  const tabUrl1 =
    "data:text/html,<html><head><title>Single process tab</title></head><body>Hello</body></html>";
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl1);
  const tabUrl2 = `data:text/html,<html>
      <head>
        <title>Multi-process tab</title>
      </head>
      <body>
        <iframe
          id="inlineFrameExample"
          title="Inline Frame Example"
          width="300"
          height="200"
          src="https://example.com">
        </iframe>
      </body>
    </html>`;
  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl2);
  const previewContainer = document.getElementById("tab-preview-panel");

  await openPreview(tab1);
  Assert.stringMatches(
    previewContainer.querySelector(".tab-preview-pid").innerText,
    /^pid: \d+$/,
    "Tab PID is shown on single process tab"
  );
  Assert.equal(
    previewContainer.querySelector(".tab-preview-activeness").innerText,
    "",
    "Tab activeness is not shown on inactive tab"
  );
  await closePreviews();

  await openPreview(tab2);
  Assert.stringMatches(
    previewContainer.querySelector(".tab-preview-pid").innerText,
    /^pids: \d+, \d+$/,
    "Tab PIDs are shown on multi-process tab"
  );
  Assert.equal(
    previewContainer.querySelector(".tab-preview-activeness").innerText,
    "[A]",
    "Tab activeness is shown on active tab"
  );
  await closePreviews();

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  await SpecialPowers.popPrefEnv();

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });
});

/**
 * Verify that non-selected tabs display a thumbnail in their preview
 * when browser.tabs.cardPreview.showThumbnails is set to true,
 * while the currently selected tab never displays a thumbnail in its preview.
 */
add_task(async function thumbnailTests() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.cardPreview.showThumbnails", true]],
  });
  const tabUrl1 = "about:blank";
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl1);
  const tabUrl2 = "about:blank";
  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl2);
  const previewPanel = document.getElementById("tab-preview-panel");

  let thumbnailUpdated = BrowserTestUtils.waitForEvent(
    previewPanel,
    "previewThumbnailUpdated",
    false,
    evt => evt.detail.thumbnail
  );
  await openPreview(tab1);
  await thumbnailUpdated;
  Assert.ok(
    previewPanel.querySelectorAll(
      ".tab-preview-thumbnail-container img, .tab-preview-thumbnail-container canvas"
    ).length,
    "Tab1 preview contains thumbnail"
  );

  await closePreviews();
  thumbnailUpdated = BrowserTestUtils.waitForEvent(
    previewPanel,
    "previewThumbnailUpdated"
  );
  await openPreview(tab2);
  await thumbnailUpdated;
  Assert.equal(
    previewPanel.querySelectorAll(
      ".tab-preview-thumbnail-container img, .tab-preview-thumbnail-container canvas"
    ).length,
    0,
    "Tab2 (selected) does not contain thumbnail"
  );

  const previewHidden = BrowserTestUtils.waitForPopupEvent(
    previewPanel,
    "hidden"
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  await SpecialPowers.popPrefEnv();

  // Removing the tab should close the preview.
  await previewHidden;

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });
});

/**
 * make sure delay is applied when mouse leaves tabstrip
 * but not when moving between tabs on the tabstrip
 */
add_task(async function delayTests() {
  const tabUrl1 =
    "data:text/html,<html><head><title>First New Tab</title></head><body>Hello</body></html>";
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl1);
  const tabUrl2 =
    "data:text/html,<html><head><title>Second New Tab</title></head><body>Hello</body></html>";
  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl2);
  const previewComponent = gBrowser.tabContainer.previewPanel;
  const previewElement = document.getElementById("tab-preview-panel");

  sinon.spy(previewComponent, "deactivate");

  await openPreview(tab1);

  // I can't fake this like in hoverTests, need to send an updated-tab signal
  //await openPreview(tab2);

  const previewHidden = BrowserTestUtils.waitForPopupEvent(
    previewElement,
    "hidden"
  );
  Assert.ok(
    !previewComponent.deactivate.called,
    "Delay is not reset when moving between tabs"
  );

  EventUtils.synthesizeMouseAtCenter(document.getElementById("reload-button"), {
    type: "mousemove",
  });

  await previewHidden;

  Assert.ok(
    previewComponent.deactivate.called,
    "Delay is reset when cursor leaves tabstrip"
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  sinon.restore();
});

/**
 * Dragging a tab should deactivate the preview
 */
add_task(async function dragTests() {
  await SpecialPowers.pushPrefEnv({
    set: [["ui.tooltip.delay_ms", 1000]],
  });
  const tabUrl1 =
    "data:text/html,<html><head><title>First New Tab</title></head><body>Hello</body></html>";
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl1);
  const tabUrl2 =
    "data:text/html,<html><head><title>Second New Tab</title></head><body>Hello</body></html>";
  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl2);
  const previewComponent = gBrowser.tabContainer.previewPanel;
  const previewElement = document.getElementById("tab-preview-panel");

  sinon.spy(previewComponent, "deactivate");

  await openPreview(tab1);
  const previewHidden = BrowserTestUtils.waitForPopupEvent(
    previewElement,
    "hidden"
  );
  let dragend = BrowserTestUtils.waitForEvent(tab1, "dragend");
  EventUtils.synthesizePlainDragAndDrop({
    srcElement: tab1,
    destElement: tab2,
  });

  await previewHidden;

  Assert.ok(
    previewComponent.deactivate.called,
    "delay is reset after drag started"
  );

  await dragend;

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  sinon.restore();

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });

  await SpecialPowers.popPrefEnv();
});

/**
 * Wheel events at the document-level of the window should hide the preview.
 */
add_task(async function wheelTests() {
  const tabUrl1 = "about:blank";
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl1);
  const tabUrl2 = "about:blank";
  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl2);

  await openPreview(tab1);

  const tabs = document.getElementById("tabbrowser-tabs");
  const previewHidden = BrowserTestUtils.waitForPopupEvent(
    document.getElementById("tab-preview-panel"),
    "hidden"
  );

  // Copied from apz_test_native_event_utils.js
  let message = 0;
  switch (AppConstants.platform) {
    case "win":
      message = 0x020a;
      break;
    case "linux":
      message = 4;
      break;
    case "macosx":
      message = 1;
      break;
  }

  let rect = tabs.getBoundingClientRect();
  let screenRect = window.windowUtils.toScreenRect(
    rect.x,
    rect.y,
    rect.width,
    rect.height
  );
  window.windowUtils.sendNativeMouseScrollEvent(
    screenRect.left,
    screenRect.bottom,
    message,
    0,
    3,
    0,
    0,
    Ci.nsIDOMWindowUtils.MOUSESCROLL_SCROLL_LINES,
    tabs,
    null
  );

  await previewHidden;

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  await SpecialPowers.popPrefEnv();

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });
});
