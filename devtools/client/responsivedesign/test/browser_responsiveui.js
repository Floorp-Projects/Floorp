/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function* () {
  let tab = yield addTab("data:text/html,mop");

  let {rdm, manager} = yield openRDM(tab, "menu");
  let container = gBrowser.getBrowserContainer();
  is(container.getAttribute("responsivemode"), "true",
     "Should be in responsive mode.");
  is(document.getElementById("menu_responsiveUI").getAttribute("checked"),
     "true", "Menu item should be checked");

  ok(rdm, "An instance of the RDM should be attached to the tab.");

  let originalWidth = (yield getSizing()).width;

  let documentLoaded = waitForDocLoadComplete();
  gBrowser.loadURI("data:text/html;charset=utf-8,mop" +
                   "<div style%3D'height%3A5000px'><%2Fdiv>");
  yield documentLoaded;

  let newWidth = (yield getSizing()).width;
  is(originalWidth, newWidth, "Floating scrollbars shouldn't change the width");

  yield testPresets(rdm, manager);

  info("Testing mouse resizing");
  yield testManualMouseResize(rdm, manager);

  info("Testing mouse resizing with shift key");
  yield testManualMouseResize(rdm, manager, "shift");

  info("Testing mouse resizing with ctrl key");
  yield testManualMouseResize(rdm, manager, "ctrl");

  info("Testing resizing with user custom keyboard input");
  yield testResizeUsingCustomInput(rdm, manager);

  info("Testing invalid keyboard input");
  yield testInvalidUserInput(rdm);

  info("Testing rotation");
  yield testRotate(rdm, manager);

  let {width: widthBeforeClose, height: heightBeforeClose} = yield getSizing();

  info("Restarting responsive mode");
  yield closeRDM(rdm);

  let resized = waitForResizeTo(manager, widthBeforeClose, heightBeforeClose);
  ({rdm} = yield openRDM(tab, "keyboard"));
  yield resized;

  let currentSize = yield getSizing();
  is(currentSize.width, widthBeforeClose, "width should be restored");
  is(currentSize.height, heightBeforeClose, "height should be restored");

  container = gBrowser.getBrowserContainer();
  is(container.getAttribute("responsivemode"), "true", "In responsive mode.");
  is(document.getElementById("menu_responsiveUI").getAttribute("checked"),
     "true", "menu item should be checked");

  let isWinXP = navigator.userAgent.indexOf("Windows NT 5.1") != -1;
  if (!isWinXP) {
    yield testScreenshot(rdm);
  }

  yield closeRDM(rdm);
  is(document.getElementById("menu_responsiveUI").getAttribute("checked"),
     "false", "menu item should be unchecked");
});

function* testPresets(rdm, manager) {
  // Starting from length - 4 because last 3 items are not presets :
  // the separator, the add button and the remove button
  for (let c = rdm.menulist.firstChild.childNodes.length - 4; c >= 0; c--) {
    let item = rdm.menulist.firstChild.childNodes[c];
    let [width, height] = extractSizeFromString(item.getAttribute("label"));
    yield setPresetIndex(rdm, manager, c);

    let {width: contentWidth, height: contentHeight} = yield getSizing();
    is(contentWidth, width, "preset" + c + ": the width should be changed");
    is(contentHeight, height, "preset" + c + ": the height should be changed");
  }
}

function* testManualMouseResize(rdm, manager, pressedKey) {
  yield setSize(rdm, manager, 100, 100);

  let {width: initialWidth, height: initialHeight} = yield getSizing();
  is(initialWidth, 100, "Width should be reset to 100");
  is(initialHeight, 100, "Height should be reset to 100");

  let x = 2, y = 2;
  EventUtils.synthesizeMouse(rdm.resizer, x, y, {type: "mousedown"}, window);

  let mouseMoveParams = {type: "mousemove"};
  if (pressedKey == "shift") {
    x += 23; y += 10;
    mouseMoveParams.shiftKey = true;
  } else if (pressedKey == "ctrl") {
    x += 120; y += 60;
    mouseMoveParams.ctrlKey = true;
  } else {
    x += 20; y += 10;
  }

  EventUtils.synthesizeMouse(rdm.resizer, x, y, mouseMoveParams, window);
  EventUtils.synthesizeMouse(rdm.resizer, x, y, {type: "mouseup"}, window);

  yield once(manager, "contentResize");

  let expectedWidth = initialWidth + 20;
  let expectedHeight = initialHeight + 10;
  info("initial width: " + initialWidth);
  info("initial height: " + initialHeight);

  yield verifyResize(rdm, expectedWidth, expectedHeight);
}

function* testResizeUsingCustomInput(rdm, manager) {
  let {width: initialWidth, height: initialHeight} = yield getSizing();
  let expectedWidth = initialWidth - 20, expectedHeight = initialHeight - 10;

  let userInput = expectedWidth + " x " + expectedHeight;
  rdm.menulist.inputField.value = "";
  rdm.menulist.focus();
  processStringAsKey(userInput);

  // While typing, the size should not change
  let currentSize = yield getSizing();
  is(currentSize.width, initialWidth, "Typing shouldn't change the width");
  is(currentSize.height, initialHeight, "Typing shouldn't change the height");

  // Only the `change` event must change the size
  EventUtils.synthesizeKey("VK_RETURN", {});

  yield once(manager, "contentResize");

  yield verifyResize(rdm, expectedWidth, expectedHeight);
}

function* testInvalidUserInput(rdm) {
  let {width: initialWidth, height: initialHeight} = yield getSizing();
  let index = rdm.menulist.selectedIndex;
  let expectedValue = initialWidth + "\u00D7" + initialHeight;
  let expectedLabel = rdm.menulist.firstChild.firstChild.getAttribute("label");

  let userInput = "I'm wrong";

  rdm.menulist.inputField.value = "";
  rdm.menulist.focus();
  processStringAsKey(userInput);
  EventUtils.synthesizeKey("VK_RETURN", {});

  let currentSize = yield getSizing();
  is(currentSize.width, initialWidth, "Width should not change");
  is(currentSize.height, initialHeight, "Height should not change");
  is(rdm.menulist.selectedIndex, index, "Selected item should not change.");
  is(rdm.menulist.value, expectedValue, "Value should be reset");

  let label = rdm.menulist.firstChild.firstChild.getAttribute("label");
  is(label, expectedLabel, "Custom menuitem's label should not change");
}

function* testRotate(rdm, manager) {
  yield setSize(rdm, manager, 100, 200);

  let {width: initialWidth, height: initialHeight} = yield getSizing();
  rdm.rotate();

  yield once(manager, "contentResize");

  let newSize = yield getSizing();
  is(newSize.width, initialHeight, "The width should now be the height.");
  is(newSize.height, initialWidth, "The height should now be the width.");

  let label = rdm.menulist.firstChild.firstChild.getAttribute("label");
  let [width, height] = extractSizeFromString(label);
  is(width, initialHeight, "Width in label should be updated");
  is(height, initialWidth, "Height in label should be updated");
}

function* verifyResize(rdm, expectedWidth, expectedHeight) {
  let currentSize = yield getSizing();
  is(currentSize.width, expectedWidth, "Width should now change");
  is(currentSize.height, expectedHeight, "Height should now change");

  is(rdm.menulist.selectedIndex, -1, "Custom menuitem cannot be selected");

  let label = rdm.menulist.firstChild.firstChild.getAttribute("label");
  let value = rdm.menulist.value;
  isnot(label, value,
        "The menulist item label should be different than the menulist value");

  let [width, height] = extractSizeFromString(label);
  is(width, expectedWidth, "Width in label should be updated");
  is(height, expectedHeight, "Height in label should be updated");

  [width, height] = extractSizeFromString(value);
  is(width, expectedWidth, "Value should be updated with new width");
  is(height, expectedHeight, "Value should be updated with new height");
}

function* testScreenshot(rdm) {
  info("Testing screenshot");
  rdm.screenshot("responsiveui");
  let {FileUtils} = Cu.import("resource://gre/modules/FileUtils.jsm", {});

  while (true) {
    // while(true) until we find the file.
    // no need for a timeout, the test will get killed anyway.
    let file = FileUtils.getFile("DfltDwnld", [ "responsiveui.png" ]);
    if (file.exists()) {
      ok(true, "Screenshot file exists");
      file.remove(false);
      break;
    }
    info("checking if file exists in 200ms");
    yield wait(200);
  }
}

function* getSizing() {
  let browser = gBrowser.selectedBrowser;
  let sizing = yield ContentTask.spawn(browser, {}, function* () {
    return {
      width: content.innerWidth,
      height: content.innerHeight
    };
  });
  return sizing;
}

function extractSizeFromString(str) {
  let numbers = str.match(/(\d+)[^\d]*(\d+)/);
  if (numbers) {
    return [numbers[1], numbers[2]];
  }
  return [null, null];
}

function processStringAsKey(str) {
  for (let i = 0, l = str.length; i < l; i++) {
    EventUtils.synthesizeKey(str.charAt(i), {});
  }
}
