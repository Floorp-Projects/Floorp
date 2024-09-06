/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that drag and drop events are sent at the right time.
// Includes tests for dragging between domains, windows and iframes.
//
// Each source and target element is from one of the
// places in the following list:
//
// 0. ".org" domain, window/tab #1
// 1. window/tab #1 iframe in same domain
// 2. ".com" domain, window/tab #2
// 3. window/tab #2 iframe in same domain
//
// To avoid symmetric tests, the drag source must be in the first window.

"use strict";

const { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

// outer-frame domains
const OUTER_BASE_1 = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.org"
);
const OUTER_BASE_2 = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
// iframe domains
const INNER_BASE_1 = OUTER_BASE_1;
const INNER_BASE_2 = OUTER_BASE_2;

// The tabs, each in their own widget.
let tab1Cxt;
let tab2Cxt;

let dragServiceCid;

// JS controller for mock drag service
let dragController;

async function runDnd(
  testName,
  sourceBrowsingCxt,
  targetBrowsingCxt,
  dndOptions = {}
) {
  return EventUtils.synthesizeMockDragAndDrop({
    dragController,
    srcElement: "dropSource",
    targetElement: "dropTarget",
    sourceBrowsingCxt,
    targetBrowsingCxt,
    id: SpecialPowers.Ci.nsIDOMWindowUtils.DEFAULT_MOUSE_POINTER_ID,
    contextLabel: testName,
    info,
    record,
    dragAction: Ci.nsIDragService.DRAGDROP_ACTION_MOVE,
    ...dndOptions,
  });
}

async function openWindow(tabIdx) {
  let win =
    tabIdx == 0 ? window : await BrowserTestUtils.openNewBrowserWindow();
  let url =
    (tabIdx == 0 ? OUTER_BASE_1 : OUTER_BASE_2) + "browser_dragdrop_outer.html";
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    url,
  });
  registerCleanupFunction(async function () {
    await BrowserTestUtils.removeTab(tab);
    if (tabIdx != 0) {
      await BrowserTestUtils.closeWindow(win);
    }
  });

  // Set the URL for the iframe.  Also set
  // neverAllowSessionIsSynthesizedForTests for both frames
  // (the second is redundant if they are in the same process).
  await SpecialPowers.spawn(
    tab.linkedBrowser.browsingContext,
    [tabIdx == 0 ? INNER_BASE_1 : INNER_BASE_2],
    iframeUrl => {
      content.document.getElementById("iframe").src =
        iframeUrl + "browser_dragdrop_inner.html";
      const ds = SpecialPowers.Cc[
        "@mozilla.org/widget/dragservice;1"
      ].getService(SpecialPowers.Ci.nsIDragService);
      ds.neverAllowSessionIsSynthesizedForTests = true;
    }
  );

  await SpecialPowers.spawn(
    tab.linkedBrowser.browsingContext.children[0],
    [],
    () => {
      const ds = SpecialPowers.Cc[
        "@mozilla.org/widget/dragservice;1"
      ].getService(SpecialPowers.Ci.nsIDragService);
      ds.neverAllowSessionIsSynthesizedForTests = true;
    }
  );

  return tab.linkedBrowser.browsingContext;
}

add_setup(async function () {
  const oldDragService = SpecialPowers.Cc[
    "@mozilla.org/widget/dragservice;1"
  ].getService(SpecialPowers.Ci.nsIDragService);
  dragController = oldDragService.getMockDragController();
  dragServiceCid = MockRegistrar.register(
    "@mozilla.org/widget/dragservice;1",
    dragController.mockDragService
  );
  ok(dragServiceCid, "MockDragService was registered");
  // If the mock failed then don't continue or else we could trigger native
  // DND behavior.
  if (!dragServiceCid) {
    SimpleTest.finish();
  }
  registerCleanupFunction(async function () {
    MockRegistrar.unregister(dragServiceCid);
  });
  dragController.mockDragService.neverAllowSessionIsSynthesizedForTests = true;

  tab1Cxt = await openWindow(0);
  tab2Cxt = await openWindow(1);
});

add_task(async function test_dnd_tab1_to_tab1() {
  await runDnd("tab1->tab1", tab1Cxt, tab1Cxt);
});

add_task(async function test_dnd_tab1_to_iframe1() {
  await runDnd("tab1->iframe1", tab1Cxt, tab1Cxt.children[0]);
});

add_task(async function test_dnd_tab1_to_tab2() {
  await runDnd("tab1->tab2", tab1Cxt, tab2Cxt);
});

add_task(async function test_dnd_tab1_to_iframe2() {
  await runDnd("tab1->iframe2", tab1Cxt, tab2Cxt.children[0]);
});

add_task(async function test_dnd_iframe1_to_tab1() {
  await runDnd("iframe1->tab1", tab1Cxt.children[0], tab1Cxt);
});

add_task(async function test_dnd_iframe1_to_iframe1() {
  await runDnd("iframe1->iframe1", tab1Cxt.children[0], tab1Cxt.children[0]);
});

add_task(async function test_dnd_iframe1_to_tab2() {
  await runDnd("iframe1->tab2", tab1Cxt.children[0], tab2Cxt);
});

add_task(async function test_dnd_iframe1_to_iframe2() {
  await runDnd("iframe1->iframe2", tab1Cxt.children[0], tab2Cxt.children[0]);
});
