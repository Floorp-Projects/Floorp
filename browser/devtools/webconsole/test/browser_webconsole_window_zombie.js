/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8,<p>test for bug 577721";

const POSITION_PREF = "devtools.webconsole.position";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", onLoad, false);
  registerCleanupFunction(testEnd);
}

function testEnd() {
  Services.prefs.clearUserPref(POSITION_PREF);
}

function onLoad() {
  browser.removeEventListener("DOMContentLoaded", onLoad, false);

  openConsole();

  let hudId = HUDService.getHudIdByWindow(content);
  let hudRef = HUDService.hudReferences[hudId];
  let hudBox = hudRef.HUDBox;

  // listen for the panel popupshown event.
  document.addEventListener("popupshown", function popupShown() {
    document.removeEventListener("popupshown", popupShown, false);

    ok(hudRef.consolePanel, "console is in a panel");

    document.addEventListener("popuphidden", function popupHidden() {
      document.removeEventListener("popuphidden", popupHidden, false);
      finishTest();
    }, false);

    // Close the window console via the menu item
    let menu = document.getElementById("webConsole");
    menu.click();
  }, false);

  hudRef.positionConsole("window");
}
