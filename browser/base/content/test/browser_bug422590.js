function test() {
  waitForExplicitFinish();
  // test the main (normal) browser window
  testCustomize(window, testChromeless);
}

function testChromeless() {
  // test a chromeless window
  var newWin = openDialog(getBrowserURL(), "_blank",
                          "chrome,dialog=no,toolbar=no", "about:blank");
  ok(newWin, "got new window");

  function runWindowTest() {
    // Check that the search bar is hidden
    var searchBar = newWin.BrowserSearch.searchBar;
    ok(searchBar, "got search bar");

    var searchBarBO = searchBar.boxObject;
    is(searchBarBO.width, 0, "search bar hidden");
    is(searchBarBO.height, 0, "search bar hidden");

    function finalize() {
      newWin.removeEventListener("load", runWindowTest, false);
      newWin.close();
      finish();
    }
    testCustomize(newWin, finalize);
  }

  newWin.addEventListener("load", runWindowTest, false);
}

function testCustomize(aWindow, aCallback) {
  var fileMenu = aWindow.document.getElementById("file-menu");
  ok(fileMenu, "got file menu");
  is(fileMenu.disabled, false, "file menu initially enabled");

  // Launch toolbar customization
  // ctEl is either iframe that contains the customize sheet, or the dialog
  var ctEl = aWindow.BrowserCustomizeToolbar();

  is(fileMenu.disabled, true,
     "file menu is disabled during toolbar customization");

  aWindow.gNavToolbox.addEventListener("beforecustomization", function () {
    aWindow.gNavToolbox.removeEventListener("beforecustomization", arguments.callee, false);
    executeSoon(ctInit);
  }, false);

  function ctInit() {
    // Close toolbar customization
    closeToolbarCustomization(aWindow, ctEl);

    // Can't use the property, since the binding may have since been removed
    // if the element is hidden (see bug 422590)
    is(fileMenu.getAttribute("disabled"), "false",
       "file menu is enabled after toolbar customization");

    if (aCallback)
      aCallback();
  }
}

function closeToolbarCustomization(aWindow, aCTWindow) {
  // Force the cleanup code to be run now instead of onunload.
  // This also hides the sheet on Mac.
  aCTWindow.finishToolbarCustomization();

  // On windows and linux, need to explicitly close the window.
  if (!gCustomizeSheet)
    aCTWindow.close();
}
