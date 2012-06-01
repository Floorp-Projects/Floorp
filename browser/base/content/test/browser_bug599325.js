function test() {
  waitForExplicitFinish();
  testCustomize(window, finish);
}

function testCustomize(aWindow, aCallback) {
  var addonBar = aWindow.document.getElementById("addon-bar");
  ok(addonBar, "got addon bar");
  ok(!isElementVisible(addonBar), "addon bar initially hidden");

  // Launch toolbar customization
  // ctEl is either iframe that contains the customize sheet, or the dialog
  var ctEl = aWindow.BrowserCustomizeToolbar();

  aWindow.gNavToolbox.addEventListener("beforecustomization", function () {
    aWindow.gNavToolbox.removeEventListener("beforecustomization", arguments.callee, false);
    executeSoon(ctInit);
  }, false);

  function ctInit() {
    ok(isElementVisible(addonBar),
       "add-on bar is visible during toolbar customization");

    // Close toolbar customization
    closeToolbarCustomization(aWindow, ctEl);

    ok(!isElementVisible(addonBar),
       "addon bar is hidden after toolbar customization");

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
