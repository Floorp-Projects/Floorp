function test() {
  var searchBar = BrowserSearch.searchBar;
  ok(searchBar, "got search bar");
  
  searchBar.focus();

  var pbo = searchBar._popup.popupBoxObject;
  ok(pbo, "popup is nsIPopupBoxObject");

  EventUtils.synthesizeKey("VK_UP", { altKey: true });
  is(pbo.popupState, "showing", "popup is opening after Alt+Up");

  EventUtils.synthesizeKey("VK_ESCAPE", {});
  is(pbo.popupState, "closed", "popup is closed after ESC");

  EventUtils.synthesizeKey("VK_DOWN", { altKey: true });
  is(pbo.popupState, "showing", "popup is opening after Alt+Down");

  EventUtils.synthesizeKey("VK_ESCAPE", {});
  is(pbo.popupState, "closed", "popup is closed after ESC 2");

  if (!/Mac/.test(navigator.platform)) {
    EventUtils.synthesizeKey("VK_F4", {});
    is(pbo.popupState, "showing", "popup is opening after F4");

    EventUtils.synthesizeKey("VK_ESCAPE", {});
    is(pbo.popupState, "closed", "popup is closed after ESC 3");
  }
}
