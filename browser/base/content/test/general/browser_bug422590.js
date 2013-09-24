function test() {
  waitForExplicitFinish();
  // test the main (normal) browser window
  testCustomize(window, testChromeless);
}

function testChromeless() {
  // test a chromeless window
  var newWin = openDialog(getBrowserURL(), "_blank",
                          "chrome,dialog=no,location=yes,toolbar=no", "about:blank");
  ok(newWin, "got new window");

  whenDelayedStartupFinished(newWin, function () {
    // Check that the search bar is hidden
    var searchBar = newWin.BrowserSearch.searchBar;
    ok(searchBar, "got search bar");

    var searchBarBO = searchBar.boxObject;
    is(searchBarBO.width, 0, "search bar hidden");
    is(searchBarBO.height, 0, "search bar hidden");

    testCustomize(newWin, function () {
      newWin.close();
      finish();
    });
  });
}

function testCustomize(aWindow, aCallback) {
  var fileMenu = aWindow.document.getElementById("file-menu");
  ok(fileMenu, "got file menu");
  is(fileMenu.disabled, false, "file menu initially enabled");

  openToolbarCustomizationUI(function () {
    // Can't use the property, since the binding may have since been removed
    // if the element is hidden (see bug 422590)
    is(fileMenu.getAttribute("disabled"), "true",
       "file menu is disabled during toolbar customization");

    closeToolbarCustomizationUI(onClose, aWindow);
  }, aWindow);

  function onClose() {
    is(fileMenu.getAttribute("disabled"), "false",
       "file menu is enabled after toolbar customization");

    if (aCallback)
      aCallback();
  }
}
