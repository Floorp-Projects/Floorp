/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test to ensure that on "mousedown" in Toolbar we set Speculative Connection
 */

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const sandbox = sinon.createSandbox();
let spy = sandbox
  .stub(PlacesUIUtils, "setupSpeculativeConnection")
  .returns(Promise.resolve());

add_setup(async function () {
  await PlacesUtils.bookmarks.eraseEverything();

  let toolbar = document.getElementById("PersonalToolbar");
  ok(toolbar, "PersonalToolbar should not be null");

  if (toolbar.collapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
    registerCleanupFunction(function () {
      return promiseSetToolbarVisibility(toolbar, false);
    });
  }
  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
    sandbox.restore();
  });
});

add_task(async function checkToolbarSpeculativeConnection() {
  let toolbarBookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "https://example.com/",
    title: "Bookmark 1",
  });
  let toolbarNode = getToolbarNodeForItemGuid(toolbarBookmark.guid);

  info("Synthesize mousedown on selected bookmark");
  EventUtils.synthesizeMouseAtCenter(toolbarNode, {
    type: "mousedown",
  });
  EventUtils.synthesizeMouseAtCenter(toolbarNode, {
    type: "mouseup",
  });

  Assert.ok(spy.called, "Speculative connection for Toolbar called");
  sandbox.restore();
});

add_task(async function checkMenuSpeculativeConnection() {
  await PlacesUtils.bookmarks.eraseEverything();

  info("Placing a Menu widget");
  let origBMBlocation = CustomizableUI.getPlacementOfWidget(
    "bookmarks-menu-button"
  );
  // Ensure BMB is available in UI.
  if (!origBMBlocation) {
    CustomizableUI.addWidgetToArea(
      "bookmarks-menu-button",
      CustomizableUI.AREA_NAVBAR
    );
  }

  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.eraseEverything();
    // if BMB was not originally in UI, remove it.
    if (!origBMBlocation) {
      CustomizableUI.removeWidgetFromArea("bookmarks-menu-button");
    }
  });

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "http://example.com/",
    title: "Bookmark 2",
  });

  // Test Bookmarks Menu Button
  let BMB = document.getElementById("bookmarks-menu-button");
  let BMBpopup = document.getElementById("BMB_bookmarksPopup");
  let promiseEvent = BrowserTestUtils.waitForEvent(BMBpopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(BMB, {});
  await promiseEvent;
  info("Popupshown on Bookmarks-Menu-Button");

  let menuBookmark = [...BMBpopup.children].find(
    node => node.label == "Bookmark 2"
  );

  EventUtils.synthesizeMouseAtCenter(menuBookmark, {
    type: "mousedown",
  });
  EventUtils.synthesizeMouseAtCenter(menuBookmark, {
    type: "mouseup",
  });

  Assert.ok(spy.called, "Speculative connection for Menu Button called");
  sandbox.restore();
});
