/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let instance, widthBeforeClose, heightBeforeClose;
  let mgr = ResponsiveUI.ResponsiveUIManager;

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(startTest, content);
  }, true);

  content.location = "data:text/html,mop";

  function startTest() {
    document.getElementById("Tools:ResponsiveUI").removeAttribute("disabled");
    mgr.once("on", function() {executeSoon(onUIOpen)});
    synthesizeKeyFromKeyTag("key_responsiveUI");
  }

  function onUIOpen() {
    // Is it open?
    let container = gBrowser.getBrowserContainer();
    is(container.getAttribute("responsivemode"), "true", "In responsive mode.");

    // Menus are correctly updated?
    is(document.getElementById("Tools:ResponsiveUI").getAttribute("checked"), "true", "menus checked");

    instance = gBrowser.selectedTab.__responsiveUI;
    ok(instance, "instance of the module is attached to the tab.");

    if (instance._floatingScrollbars) {
      ensureScrollbarsAreFloating();
    }

    instance.transitionsEnabled = false;

    testPresets();
  }

  function ensureScrollbarsAreFloating() {
    let body = gBrowser.contentDocument.body;
    let html = gBrowser.contentDocument.documentElement;

    let originalWidth = body.getBoundingClientRect().width;

    html.style.overflowY = "scroll"; // Force scrollbars
    // Flush. Should not be needed as getBoundingClientRect() should flush,
    // but just in case.
    gBrowser.contentWindow.getComputedStyle(html).overflowY;
    let newWidth = body.getBoundingClientRect().width;
    is(originalWidth, newWidth, "Floating scrollbars are presents");
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
    // Starting from length - 4 because last 3 items are not presets : separator, addbutton and removebutton
    testOnePreset(instance.menulist.firstChild.childNodes.length - 4);
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

    info("XXX BUG 851296: instance.closing: " + !!instance.closing);

    mgr.once("off", function() {
      info("XXX BUG 851296: 'off' received.");
      executeSoon(restart);
    });
    EventUtils.synthesizeKey("VK_ESCAPE", {});
  }

  function restart() {
    info("XXX BUG 851296: restarting.");
    info("XXX BUG 851296: __responsiveUI: " + gBrowser.selectedTab.__responsiveUI);
    mgr.once("on", function() {
      info("XXX BUG 851296: 'on' received.");
      executeSoon(onUIOpen2);
    });
    //XXX BUG 851296: synthesizeKeyFromKeyTag("key_responsiveUI");
    mgr.toggle(window, gBrowser.selectedTab);
    info("XXX BUG 851296: restart() finished.");
  }

  function onUIOpen2() {
    info("XXX BUG 851296: onUIOpen2.");
    let container = gBrowser.getBrowserContainer();
    is(container.getAttribute("responsivemode"), "true", "In responsive mode.");

    // Menus are correctly updated?
    is(document.getElementById("Tools:ResponsiveUI").getAttribute("checked"), "true", "menus checked");

    is(content.innerWidth, widthBeforeClose, "width restored.");
    is(content.innerHeight, heightBeforeClose, "height restored.");

    mgr.once("off", function() {executeSoon(finishUp)});
    EventUtils.synthesizeKey("VK_ESCAPE", {});
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

    info("XXX BUG 851296: key name: " + name);
    info("XXX BUG 851296: key modifiers: " + JSON.stringify(modifiers));
    EventUtils.synthesizeKey(name, modifiers);
  }
}
