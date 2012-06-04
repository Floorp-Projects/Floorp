/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let instance, widthBeforeClose, heightBeforeClose;

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(startTest, content);
  }, true);

  content.location = "data:text/html,mop";

  function startTest() {
    document.getElementById("Tools:ResponsiveUI").removeAttribute("disabled");
    synthesizeKeyFromKeyTag("key_responsiveUI");
    executeSoon(onUIOpen);
  }

  function onUIOpen() {
    // Is it open?
    let container = gBrowser.getBrowserContainer();
    is(container.getAttribute("responsivemode"), "true", "In responsive mode.");

    // Menus are correctly updated?
    is(document.getElementById("Tools:ResponsiveUI").getAttribute("checked"), "true", "menus checked");

    instance = gBrowser.selectedTab.responsiveUI;
    ok(instance, "instance of the module is attached to the tab.");

    instance.transitionsEnabled = false;

    testPresets();
  }

  function testPresets() {
    function testOnePreset(c) {
      if (c == 0) {
        executeSoon(testCustom);
        return;
      }
      instance.menulist.selectedIndex = c;
      let item = instance.menulist.firstChild.childNodes[c];
      let [width, height] = extractSizeFromString(item.getAttribute("label"));
      is(content.innerWidth, width, "preset " + c + ": dimension valid (width)");
      is(content.innerHeight, height, "preset " + c + ": dimension valid (height)");

      testOnePreset(c - 1);
    }
    testOnePreset(instance.menulist.firstChild.childNodes.length - 1);
  }

  function extractSizeFromString(str) {
    let numbers = str.match(/(\d+)[^\d]*(\d+)/);
    if (numbers) {
      return [numbers[1], numbers[2]];
    } else {
      return [null, null];
    }
  }

  function testCustom() {
    let initialWidth = content.innerWidth;
    let initialHeight = content.innerHeight;

    let x = 2, y = 2;
    EventUtils.synthesizeMouse(instance.resizer, x, y, {type: "mousedown"}, window);
    x += 20; y += 10;
    EventUtils.synthesizeMouse(instance.resizer, x, y, {type: "mousemove"}, window);
    EventUtils.synthesizeMouse(instance.resizer, x, y, {type: "mouseup"}, window);

    let expectedWidth = initialWidth + 20;
    let expectedHeight = initialHeight + 10;
    info("initial width: " + initialWidth);
    info("initial height: " + initialHeight);
    is(content.innerWidth, expectedWidth, "Size correcty updated (width).");
    is(content.innerHeight, expectedHeight, "Size correcty updated (height).");
    is(instance.menulist.selectedIndex, 0, "Custom menuitem selected");
    let [width, height] = extractSizeFromString(instance.menulist.firstChild.firstChild.getAttribute("label"));
    is(width, expectedWidth, "Label updated (width).");
    is(height, expectedHeight, "Label updated (height).");
    rotate();
  }

  function rotate() {
    let initialWidth = content.innerWidth;
    let initialHeight = content.innerHeight;

    info("rotate");
    instance.rotate();

    is(content.innerWidth, initialHeight, "The width is now the height.");
    is(content.innerHeight, initialWidth, "The height is now the width.");
    let [width, height] = extractSizeFromString(instance.menulist.firstChild.firstChild.getAttribute("label"));
    is(width, initialHeight, "Label updated (width).");
    is(height, initialWidth, "Label updated (height).");

    widthBeforeClose = content.innerWidth;
    heightBeforeClose = content.innerHeight;

    EventUtils.synthesizeKey("VK_ESCAPE", {});

    executeSoon(restart);
  }

  function restart() {
    synthesizeKeyFromKeyTag("key_responsiveUI");
    executeSoon(onUIOpen2);
  }

  function onUIOpen2() {
    let container = gBrowser.getBrowserContainer();
    is(container.getAttribute("responsivemode"), "true", "In responsive mode.");

    // Menus are correctly updated?
    is(document.getElementById("Tools:ResponsiveUI").getAttribute("checked"), "true", "menus checked");

    is(content.innerWidth, widthBeforeClose, "width restored.");
    is(content.innerHeight, heightBeforeClose, "height restored.");

    EventUtils.synthesizeKey("VK_ESCAPE", {});
    executeSoon(finishUp);
  }

  function finishUp() {

    // Menus are correctly updated?
    is(document.getElementById("Tools:ResponsiveUI").getAttribute("checked"), "false", "menu unchecked");

    delete instance;
    gBrowser.removeCurrentTab();
    finish();
  }

  function synthesizeKeyFromKeyTag(aKeyId) {
    let key = document.getElementById(aKeyId);
    isnot(key, null, "Successfully retrieved the <key> node");

    let modifiersAttr = key.getAttribute("modifiers");

    let name = null;

    if (key.getAttribute("keycode"))
      name = key.getAttribute("keycode");
    else if (key.getAttribute("key"))
      name = key.getAttribute("key");

    isnot(name, null, "Successfully retrieved keycode/key");

    let modifiers = {
      shiftKey: modifiersAttr.match("shift"),
      ctrlKey: modifiersAttr.match("ctrl"),
      altKey: modifiersAttr.match("alt"),
      metaKey: modifiersAttr.match("meta"),
      accelKey: modifiersAttr.match("accel")
    }

    EventUtils.synthesizeKey(name, modifiers);
  }
}
