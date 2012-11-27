/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8,<p>test for bug 577721";

const POSITION_PREF = "devtools.webconsole.position";
const TOP_PREF = "devtools.webconsole.top";
const LEFT_PREF = "devtools.webconsole.left";
const WIDTH_PREF = "devtools.webconsole.width";
const HEIGHT_PREF = "devtools.hud.height";

let hudRef, boxHeight, panelWidth;

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
  registerCleanupFunction(testEnd);
}

function testEnd() {
  hudRef = null;
  Services.prefs.clearUserPref(POSITION_PREF);
  Services.prefs.clearUserPref(WIDTH_PREF);
  Services.prefs.clearUserPref(HEIGHT_PREF);
  Services.prefs.clearUserPref(TOP_PREF);
  Services.prefs.clearUserPref(LEFT_PREF);
}

function waitForPosition(aPosition, aCallback) {
  waitForSuccess({
    name: "web console position changed to '" + aPosition + "'",
    validatorFn: function()
    {
      return hudRef._currentUIPosition == aPosition;
    },
    successFn: executeSoon.bind(null, aCallback),
    failureFn: finishTest,
  });
}

function consoleOpened(aHudRef) {
  hudRef = aHudRef;
  testMenuitems();

  let hudBox = hudRef.iframe;

  is(hudBox.parentNode.childNodes[2].getAttribute("id"), hudRef.hudId,
     "initial console position is correct");

  is(hudRef.ui.positionMenuitems.below.getAttribute("checked"), "true",
     "position menu checkbox is below");
  is(Services.prefs.getCharPref(POSITION_PREF), "below", "pref is below");

  executeSoon(function() {
    hudRef.positionConsole("above");
    waitForPosition("above", onPositionAbove);
  });
}

function onPositionAbove() {
  let hudBox = hudRef.iframe;

  let id = hudBox.parentNode.childNodes[0].getAttribute("id");
  is(id, hudRef.hudId, "above position is correct");

  is(hudRef.ui.positionMenuitems.above.getAttribute("checked"), "true",
     "position menu checkbox is above");
  is(Services.prefs.getCharPref(POSITION_PREF), "above", "pref is above");

  boxHeight = content.innerHeight * 0.5;
  panelWidth = content.innerWidth * 0.5;

  hudBox.style.height = boxHeight + "px";

  boxHeight = hudBox.clientHeight;

  Services.prefs.setIntPref(WIDTH_PREF, panelWidth);
  Services.prefs.setIntPref(TOP_PREF, 50);
  Services.prefs.setIntPref(LEFT_PREF, 51);

  executeSoon(function() {
    hudRef.positionConsole("window");
    waitForPosition("window", onPositionWindow);
  });
}

function onPositionWindow() {
  let hudBox = hudRef.iframe;

  let id = hudBox.parentNode.getAttribute("id");
  is(id, "console_window_" + hudRef.hudId, "window position is correct");
  is(Services.prefs.getCharPref(POSITION_PREF), "window", "pref is window");

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
    waitForPosition("below", onPositionBelow);
  });
}

function onPositionBelow() {
  let hudBox = hudRef.iframe;

  let id = hudBox.parentNode.childNodes[2].getAttribute("id");
  is(id, hudRef.hudId, "below position is correct after reopen");

  let diffHeight = Math.abs(hudBox.clientHeight - boxHeight);
  // dump("Diffheight: " + diffHeight + " clientHeight: " + hudBox.clientHeight + " boxHeight: " + boxHeight + "\n");
  // XXX TODO bug 702707
  ok(diffHeight < 8, "hudBox height is still correct");

  is(Services.prefs.getCharPref(POSITION_PREF), "below", "pref is below");

  // following three disabled due to bug 674562
  // is(Services.prefs.getIntPref(WIDTH_PREF), panelWidth, "width pref updated - bug 674562");
  // isnot(Services.prefs.getIntPref(TOP_PREF), 50, "top location pref updated - bug 674562");
  // isnot(Services.prefs.getIntPref(LEFT_PREF), 51, "left location pref updated - bug 674562");

  Services.obs.addObserver(onConsoleClose, "web-console-destroyed", false);

  // Close the window console via the toolbar button
  let btn = hudRef.ui.closeButton;
  executeSoon(function() {
    EventUtils.synthesizeMouse(btn, 2, 2, {}, hudRef.iframeWindow);
  });
}

function onConsoleClose()
{
  Services.obs.removeObserver(onConsoleClose, "web-console-destroyed");

  executeSoon(function() {
    hudRef = null;
    openConsole(null, onConsoleReopen);
  });
}

function onConsoleReopen(aHudRef) {
  let hudBox = aHudRef.iframe;

  let id = hudBox.parentNode.childNodes[2].getAttribute("id");
  is(id, aHudRef.hudId, "below position is correct after another reopen");

  is(aHudRef.ui.positionMenuitems.below.getAttribute("checked"), "true",
     "position menu checkbox is below");

  executeSoon(finishTest);
}

function testMenuitems() {
  let positionConsole = hudRef.positionConsole;
  is(typeof positionConsole, "function", "positionConsole() is available");

  let param = null;
  hudRef.positionConsole = function(aPosition) {
    param = aPosition;
  };

  // Make sure the menuitems call the correct method.

  hudRef.ui.positionMenuitems.above.doCommand();

  is(param, "above", "menuitem for above positioning calls positionConsole() correctly");

  hudRef.ui.positionMenuitems.below.doCommand();

  is(param, "below", "menuitem for below positioning calls positionConsole() correctly");

  hudRef.ui.positionMenuitems.window.doCommand();

  is(param, "window", "menuitem for window positioning calls positionConsole() correctly");

  hudRef.positionConsole = positionConsole;
}

