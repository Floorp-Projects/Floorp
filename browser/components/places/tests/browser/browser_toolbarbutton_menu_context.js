let bookmarksMenuButton = document.getElementById("bookmarks-menu-button");
let BMB_menuPopup = document.getElementById("BMB_bookmarksPopup");
let BMB_showAllBookmarks = document.getElementById("BMB_bookmarksShowAll");
let contextMenu = document.getElementById("placesContext");
let newBookmarkItem = document.getElementById("placesContext_new:bookmark");

waitForExplicitFinish();
add_task(function testPopup() {
  info("Checking popup context menu before moving the bookmarks button");
  yield checkPopupContextMenu();
  let pos = CustomizableUI.getPlacementOfWidget("bookmarks-menu-button").position;
  CustomizableUI.addWidgetToArea("bookmarks-menu-button", CustomizableUI.AREA_PANEL);
  CustomizableUI.addWidgetToArea("bookmarks-menu-button", CustomizableUI.AREA_NAVBAR, pos);
  info("Checking popup context menu after moving the bookmarks button");
  yield checkPopupContextMenu();
});

function* checkPopupContextMenu() {
  let dropmarker = document.getAnonymousElementByAttribute(bookmarksMenuButton, "anonid", "dropmarker");
  BMB_menuPopup.setAttribute("style", "transition: none;");
  let popupShownPromise = onPopupEvent(BMB_menuPopup, "shown");
  EventUtils.synthesizeMouseAtCenter(dropmarker, {});
  info("Waiting for bookmarks menu to be shown.");
  yield popupShownPromise;
  let contextMenuShownPromise = onPopupEvent(contextMenu, "shown");
  EventUtils.synthesizeMouseAtCenter(BMB_showAllBookmarks, {type: "contextmenu", button: 2 });
  info("Waiting for context menu on bookmarks menu to be shown.");
  yield contextMenuShownPromise;
  ok(!newBookmarkItem.hasAttribute("disabled"), "New bookmark item shouldn't be disabled");
  let contextMenuHiddenPromise = onPopupEvent(contextMenu, "hidden");
  contextMenu.hidePopup();
  BMB_menuPopup.removeAttribute("style");
  info("Waiting for context menu on bookmarks menu to be hidden.");
  yield contextMenuHiddenPromise;
  let popupHiddenPromise = onPopupEvent(BMB_menuPopup, "hidden");
  // Can't use synthesizeMouseAtCenter because the dropdown panel is in the way
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  info("Waiting for bookmarks menu to be hidden.");
  yield popupHiddenPromise;
}

function onPopupEvent(popup, evt) {
  let fullEvent = "popup" + evt;
  let deferred = new Promise.defer();
  let onPopupHandler = (e) => {
    if (e.target == popup) {
      popup.removeEventListener(fullEvent, onPopupHandler);
      deferred.resolve();
    }
  };
  popup.addEventListener(fullEvent, onPopupHandler);
  return deferred.promise;
}
