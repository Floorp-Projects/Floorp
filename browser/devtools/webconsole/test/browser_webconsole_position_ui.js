/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8,<p>test for bug 577721";

const POSITION_PREF = "devtools.webconsole.position";
const TOP_PREF = "devtools.webconsole.top";
const LEFT_PREF = "devtools.webconsole.left";
const WIDTH_PREF = "devtools.webconsole.width";
const HEIGHT_PREF = "devtools.hud.height";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", onLoad, false);
  registerCleanupFunction(testEnd);
}

function testEnd() {
  Services.prefs.clearUserPref(POSITION_PREF);
  Services.prefs.clearUserPref(WIDTH_PREF);
  Services.prefs.clearUserPref(HEIGHT_PREF);
  Services.prefs.clearUserPref(TOP_PREF);
  Services.prefs.clearUserPref(LEFT_PREF);
}

function onLoad() {
  browser.removeEventListener("DOMContentLoaded", onLoad, false);

  openConsole();

  testMenuitems();

  let hudId = HUDService.getHudIdByWindow(content);
  let hudRef = HUDService.hudReferences[hudId];
  let hudBox = hudRef.HUDBox;

  is(hudBox.parentNode.childNodes[2].getAttribute("id"), hudId,
     "initial console position is correct");

  is(hudRef.positionMenuitems.below.getAttribute("checked"), "true",
     "position menu checkbox is below");
  is(Services.prefs.getCharPref(POSITION_PREF), "below", "pref is below");

  hudRef.positionConsole("above");
  let id = hudBox.parentNode.childNodes[0].getAttribute("id");
  is(id, hudId, "above position is correct");

  is(hudRef.positionMenuitems.above.getAttribute("checked"), "true",
     "position menu checkbox is above");
  is(Services.prefs.getCharPref(POSITION_PREF), "above", "pref is above");

  // listen for the panel popupshown event.
  document.addEventListener("popupshown", function popupShown() {
    document.removeEventListener("popupshown", popupShown, false);

    document.addEventListener("popuphidden", function popupHidden() {
      document.removeEventListener("popuphidden", popupHidden, false);

      id = hudBox.parentNode.childNodes[2].getAttribute("id");
      is(id, hudId, "below position is correct after reopen");

      diffHeight = Math.abs(hudBox.clientHeight - boxHeight);
      // dump("Diffheight: " + diffHeight + " clientHeight: " + hudBox.clientHeight + " boxHeight: " + boxHeight + "\n");
      // XXX TODO bug 702707
      todo(diffHeight < 3, "hudBox height is still correct");

      is(Services.prefs.getCharPref(POSITION_PREF), "below", "pref is below");

      // following three disabled due to bug 674562
      // is(Services.prefs.getIntPref(WIDTH_PREF), panelWidth, "width pref updated - bug 674562");
      // isnot(Services.prefs.getIntPref(TOP_PREF), 50, "top location pref updated - bug 674562");
      // isnot(Services.prefs.getIntPref(LEFT_PREF), 51, "left location pref updated - bug 674562");

      // Close the window console via the toolbar button
      let btn = hudBox.querySelector(".webconsole-close-button");
      EventUtils.sendMouseEvent({ type: "click" }, btn);

      openConsole();

      hudId = HUDService.getHudIdByWindow(content);
      hudRef = HUDService.hudReferences[hudId];
      hudBox = hudRef.HUDBox;

      id = hudBox.parentNode.childNodes[2].getAttribute("id");
      is(id, hudId, "below position is correct after another reopen");

      is(hudRef.positionMenuitems.below.getAttribute("checked"), "true",
         "position menu checkbox is below");

      executeSoon(finishTest);
    }, false);

    let diffHeight = Math.abs(hudBox.clientHeight - boxHeight);
    ok(diffHeight < 8, "hudBox height is correct");

    let consolePanel = hudRef.consolePanel;

    is(consolePanel.getAttribute("width"), panelWidth, "panel width is correct");
    is(consolePanel.getAttribute("top"), 50, "panel top position is correct");
    is(consolePanel.getAttribute("left"), 51, "panel left position is correct");

    let panelHeight = parseInt(consolePanel.getAttribute("height"));
    let boxWidth = hudBox.clientWidth;
    boxHeight = hudBox.clientHeight;

    hudRef.consolePanel.sizeTo(panelWidth - 15, panelHeight - 13);

    let popupBoxObject = consolePanel.popupBoxObject;
    let screenX = popupBoxObject.screenX;
    let screenY = popupBoxObject.screenY;
    consolePanel.moveTo(screenX - 11, screenY - 13);

    isnot(hudBox.clientWidth, boxWidth, "hudBox width was updated");
    isnot(hudBox.clientHeight, boxHeight, "hudBox height was updated");

    isnot(popupBoxObject.screenX, screenX, "panel screenX was updated");
    isnot(popupBoxObject.screenY, screenY, "panel screenY was updated");

    panelWidth = consolePanel.clientWidth;
    boxHeight = hudBox.clientHeight;

    executeSoon(function() {
      hudRef.positionConsole("below");
    });
  }, false);

  let boxHeight = content.innerHeight * 0.5;
  let panelWidth = content.innerWidth * 0.5;

  hudBox.style.height = boxHeight + "px";

  boxHeight = hudBox.clientHeight;

  Services.prefs.setIntPref(WIDTH_PREF, panelWidth);
  Services.prefs.setIntPref(TOP_PREF, 50);
  Services.prefs.setIntPref(LEFT_PREF, 51);

  hudRef.positionConsole("window");
  id = hudBox.parentNode.getAttribute("id");
  is(id, "console_window_" + hudId, "window position is correct");
  is(Services.prefs.getCharPref(POSITION_PREF), "window", "pref is window");
}

function testMenuitems() {
  let hudId = HUDService.getHudIdByWindow(content);
  let hudRef = HUDService.hudReferences[hudId];
  let hudBox = hudRef.HUDBox;

  let positionConsole = hudRef.positionConsole;
  is(typeof positionConsole, "function", "positionConsole() is available");

  let param = null;
  hudRef.positionConsole = function(aPosition) {
    param = aPosition;
  };

  // Make sure the menuitems call the correct method.

  hudRef.positionMenuitems.above.doCommand();

  is(param, "above", "menuitem for above positioning calls positionConsole() correctly");

  hudRef.positionMenuitems.below.doCommand();

  is(param, "below", "menuitem for below positioning calls positionConsole() correctly");

  hudRef.positionMenuitems.window.doCommand();

  is(param, "window", "menuitem for window positioning calls positionConsole() correctly");

  hudRef.positionConsole = positionConsole;
}

