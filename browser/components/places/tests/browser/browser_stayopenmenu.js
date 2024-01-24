/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Menus should stay open (if pref is set) after ctrl-click, middle-click,
// and contextmenu's "Open in a new tab" click.

async function locateBookmarkAndTestCtrlClick(menupopup) {
  let testMenuitem = [...menupopup.children].find(
    node => node.label == "Test1"
  );
  ok(testMenuitem, "Found test bookmark.");
  ok(BrowserTestUtils.isVisible(testMenuitem), "Should be visible");
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, null);
  EventUtils.synthesizeMouseAtCenter(testMenuitem, { accelKey: true });
  let newTab = await promiseTabOpened;
  ok(true, "Bookmark ctrl-click opened new tab.");
  BrowserTestUtils.removeTab(newTab);
  return testMenuitem;
}

async function testContextmenu(menuitem) {
  let doc = menuitem.ownerDocument;
  let cm = doc.getElementById("placesContext");
  let promiseEvent = BrowserTestUtils.waitForEvent(cm, "popupshown");
  EventUtils.synthesizeMouseAtCenter(menuitem, {
    type: "contextmenu",
    button: 2,
  });
  await promiseEvent;
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, null);
  let hidden = BrowserTestUtils.waitForEvent(cm, "popuphidden");
  cm.activateItem(doc.getElementById("placesContext_open:newtab"));
  await hidden;
  let newTab = await promiseTabOpened;
  return newTab;
}

add_setup(async function () {
  // Ensure BMB is available in UI.
  let origBMBlocation = CustomizableUI.getPlacementOfWidget(
    "bookmarks-menu-button"
  );
  if (!origBMBlocation) {
    CustomizableUI.addWidgetToArea(
      "bookmarks-menu-button",
      CustomizableUI.AREA_NAVBAR
    );
  }

  await SpecialPowers.pushPrefEnv({
    set: [["browser.bookmarks.openInTabClosesMenu", false]],
  });
  // Ensure menubar visible.
  let menubar = document.getElementById("toolbar-menubar");
  let menubarVisible = isToolbarVisible(menubar);
  if (!menubarVisible) {
    setToolbarVisibility(menubar, true);
    info("Menubar made visible");
  }
  // Ensure Bookmarks Toolbar Visible.
  let toolbar = document.getElementById("PersonalToolbar");
  let toolbarHidden = toolbar.collapsed;
  if (toolbarHidden) {
    await promiseSetToolbarVisibility(toolbar, true);
    info("Bookmarks toolbar made visible");
  }
  // Create our test bookmarks.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "http://example.com/",
    title: "Test1",
  });
  let folder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "TEST_TITLE",
    index: 0,
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: folder.guid,
    url: "http://example.com/",
    title: "Test1",
  });

  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.eraseEverything();
    // if BMB was not originally in UI, remove it.
    if (!origBMBlocation) {
      CustomizableUI.removeWidgetFromArea("bookmarks-menu-button");
    }
    // Restore menubar to original visibility.
    setToolbarVisibility(menubar, menubarVisible);
    // Restore original bookmarks toolbar visibility.
    if (toolbarHidden) {
      await promiseSetToolbarVisibility(toolbar, false);
    }
  });
});

add_task(async function testStayopenBookmarksClicks() {
  // Test Bookmarks Menu Button stayopen clicks - Ctrl-click.
  let BMB = document.getElementById("bookmarks-menu-button");
  let BMBpopup = document.getElementById("BMB_bookmarksPopup");
  let promiseEvent = BrowserTestUtils.waitForEvent(BMBpopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(BMB, {});
  await promiseEvent;
  info("Popupshown on Bookmarks-Menu-Button");
  var menuitem = await locateBookmarkAndTestCtrlClick(BMBpopup);
  ok(BMB.open, "Bookmarks Menu Button's Popup should still be open.");

  // Test Bookmarks Menu Button stayopen clicks: middle-click.
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, null);
  EventUtils.synthesizeMouseAtCenter(menuitem, { button: 1 });
  let newTab = await promiseTabOpened;
  ok(true, "Bookmark middle-click opened new tab.");
  BrowserTestUtils.removeTab(newTab);
  ok(BMB.open, "Bookmarks Menu Button's Popup should still be open.");

  // Test Bookmarks Menu Button stayopen clicks - 'Open in new tab' on context menu.
  newTab = await testContextmenu(menuitem);
  ok(true, "Bookmark contextmenu opened new tab.");
  ok(BMB.open, "Bookmarks Menu Button's Popup should still be open.");
  promiseEvent = BrowserTestUtils.waitForEvent(BMBpopup, "popuphidden");
  BMB.open = false;
  await promiseEvent;
  info("Closing menu");
  BrowserTestUtils.removeTab(newTab);

  // Test App Menu's Bookmarks Library stayopen clicks.
  let appMenu = document.getElementById("PanelUI-menu-button");
  let appMenuPopup = document.getElementById("appMenu-popup");
  let PopupShownPromise = BrowserTestUtils.waitForEvent(
    appMenuPopup,
    "popupshown"
  );
  appMenu.click();
  await PopupShownPromise;

  let BMview;
  document.getElementById("appMenu-bookmarks-button").click();
  BMview = document.getElementById("PanelUI-bookmarks");
  let promise = BrowserTestUtils.waitForEvent(BMview, "ViewShown");
  await promise;
  info("Bookmarks panel shown.");

  // Test App Menu's Bookmarks Library stayopen clicks: Ctrl-click.
  let menu = document.getElementById("panelMenu_bookmarksMenu");
  var testMenuitem = await locateBookmarkAndTestCtrlClick(menu);
  ok(appMenu.open, "Menu should remain open.");

  // Test App Menu's Bookmarks Library stayopen clicks: middle-click.
  promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, null);
  EventUtils.synthesizeMouseAtCenter(testMenuitem, { button: 1 });
  newTab = await promiseTabOpened;
  ok(true, "Bookmark middle-click opened new tab.");
  BrowserTestUtils.removeTab(newTab);
  ok(
    PanelView.forNode(BMview).active,
    "Should still show the bookmarks subview"
  );
  ok(appMenu.open, "Menu should remain open.");

  // Close the App Menu
  appMenuPopup.hidePopup();
  ok(!appMenu.open, "The menu should now be closed.");

  // Disable the rest of the tests on Mac due to Mac's handling of menus being
  // slightly different to the other platforms.
  if (AppConstants.platform === "macosx") {
    return;
  }

  // Test Bookmarks Menu (menubar) stayopen clicks: Ctrl-click.
  let BM = document.getElementById("bookmarksMenu");
  let BMpopup = document.getElementById("bookmarksMenuPopup");
  promiseEvent = BrowserTestUtils.waitForEvent(BMpopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(BM, {});
  await promiseEvent;
  info("Popupshowing on Bookmarks Menu");
  menuitem = await locateBookmarkAndTestCtrlClick(BMpopup);
  ok(BM.open, "Bookmarks Menu's Popup should still be open.");

  // Test Bookmarks Menu (menubar) stayopen clicks: middle-click.
  promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, null);
  EventUtils.synthesizeMouseAtCenter(menuitem, { button: 1 });
  newTab = await promiseTabOpened;
  ok(true, "Bookmark middle-click opened new tab.");
  BrowserTestUtils.removeTab(newTab);
  ok(BM.open, "Bookmarks Menu's Popup should still be open.");

  // Test Bookmarks Menu (menubar) stayopen clicks: 'Open in new tab' on context menu.
  newTab = await testContextmenu(menuitem);
  ok(true, "Bookmark contextmenu opened new tab.");
  BrowserTestUtils.removeTab(newTab);
  ok(BM.open, "Bookmarks Menu's Popup should still be open.");
  promiseEvent = BrowserTestUtils.waitForEvent(BMpopup, "popuphidden");
  BM.open = false;
  await promiseEvent;

  // Test Bookmarks Toolbar stayopen clicks - Ctrl-click.
  let BT = document.getElementById("PlacesToolbarItems");
  let toolbarbutton = BT.firstElementChild;
  ok(toolbarbutton, "Folder should be first item on Bookmarks Toolbar.");
  let buttonMenupopup = toolbarbutton.firstElementChild;
  Assert.equal(
    buttonMenupopup.tagName,
    "menupopup",
    "Found toolbar button's menupopup."
  );
  promiseEvent = BrowserTestUtils.waitForEvent(buttonMenupopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(toolbarbutton, {});
  await promiseEvent;
  ok(true, "Bookmarks toolbar folder's popup is open.");
  menuitem = buttonMenupopup.firstElementChild.nextElementSibling;
  promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, null);
  EventUtils.synthesizeMouseAtCenter(menuitem, { ctrlKey: true });
  newTab = await promiseTabOpened;
  ok(
    true,
    "Bookmark in folder on bookmark's toolbar ctrl-click opened new tab."
  );
  ok(
    toolbarbutton.open,
    "Popup of folder on bookmark's toolbar should still be open."
  );
  promiseEvent = BrowserTestUtils.waitForEvent(buttonMenupopup, "popuphidden");
  toolbarbutton.open = false;
  await promiseEvent;
  BrowserTestUtils.removeTab(newTab);

  // Test Bookmarks Toolbar stayopen clicks: middle-click.
  promiseEvent = BrowserTestUtils.waitForEvent(buttonMenupopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(toolbarbutton, {});
  await promiseEvent;
  ok(true, "Bookmarks toolbar folder's popup is open.");
  promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, null);
  EventUtils.synthesizeMouseAtCenter(menuitem, { button: 1 });
  newTab = await promiseTabOpened;
  ok(
    true,
    "Bookmark in folder on Bookmarks Toolbar middle-click opened new tab."
  );
  ok(
    toolbarbutton.open,
    "Popup of folder on bookmark's toolbar should still be open."
  );
  promiseEvent = BrowserTestUtils.waitForEvent(buttonMenupopup, "popuphidden");
  toolbarbutton.open = false;
  await promiseEvent;
  BrowserTestUtils.removeTab(newTab);

  // Test Bookmarks Toolbar stayopen clicks: 'Open in new tab' on context menu.
  promiseEvent = BrowserTestUtils.waitForEvent(buttonMenupopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(toolbarbutton, {});
  await promiseEvent;
  ok(true, "Bookmarks toolbar folder's popup is open.");
  newTab = await testContextmenu(menuitem);
  ok(true, "Bookmark on Bookmarks Toolbar contextmenu opened new tab.");
  ok(
    toolbarbutton.open,
    "Popup of folder on bookmark's toolbar should still be open."
  );
  promiseEvent = BrowserTestUtils.waitForEvent(buttonMenupopup, "popuphidden");
  toolbarbutton.open = false;
  await promiseEvent;
  BrowserTestUtils.removeTab(newTab);
});
