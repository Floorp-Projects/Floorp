function test() {
  waitForExplicitFinish();
  
  // test the main (normal) browser window
  testCustomize(window, testChromeless);
}

function testChromeless() {
  // test a chromeless window
  var newWin = openDialog("chrome://browser/content/", "_blank",
                      "chrome,dialog=no,toolbar=no", "about:blank");
  ok(newWin, "got new window");

  function runWindowTest() {
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
  var addonBar = aWindow.document.getElementById("addon-bar");
  ok(addonBar, "got addon bar");
  is(addonBar.collapsed, true, "addon bar initially disabled");

  // Launch toolbar customization
  // ctEl is either iframe that contains the customize sheet, or the dialog
  var ctEl = aWindow.BrowserCustomizeToolbar();

  is(addonBar.collapsed, false,
     "file menu is not collapsed during toolbar customization");

  aWindow.gNavToolbox.addEventListener("beforecustomization", function () {
    aWindow.gNavToolbox.removeEventListener("beforecustomization", arguments.callee, false);
    executeSoon(ctInit);
  }, false);

  function ctInit() {
    // Close toolbar customization
    closeToolbarCustomization(aWindow, ctEl);

    is(addonBar.getAttribute("collapsed"), "true",
       "addon bar is collapsed after toolbar customization");

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
