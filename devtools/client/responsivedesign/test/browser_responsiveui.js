/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

thisTestLeaksUncaughtRejectionsAndShouldBeFixed("Protocol error (unknownError): Error: Got an invalid root window in DocumentWalker");

function test() {
  waitForExplicitFinish();
  SimpleTest.requestCompleteLog();
  Task.spawn(function*() {

    function extractSizeFromString(str) {
      let numbers = str.match(/(\d+)[^\d]*(\d+)/);
      if (numbers) {
        return [numbers[1], numbers[2]];
      } else {
        return [null, null];
      }
    }

    function processStringAsKey(str) {
      for (let i = 0, l = str.length; i < l; i++) {
        EventUtils.synthesizeKey(str.charAt(i), {});
      }
    }

    yield addTab("data:text/html,mop");

    let mgr = ResponsiveUI.ResponsiveUIManager;

    synthesizeKeyFromKeyTag("key_responsiveUI");

    yield once(mgr, "on");

    // Is it open?
    let container = gBrowser.getBrowserContainer();
    is(container.getAttribute("responsivemode"), "true", "In responsive mode.");

    // Menus are correctly updated?
    is(document.getElementById("Tools:ResponsiveUI").getAttribute("checked"), "true", "menus checked");

    let instance = mgr.getResponsiveUIForTab(gBrowser.selectedTab);
    ok(instance, "instance of the module is attached to the tab.");

    let originalWidth = content.innerWidth;

    let documentLoaded = waitForDocLoadComplete();
    content.location = "data:text/html;charset=utf-8,mop<div style%3D'height%3A5000px'><%2Fdiv>";
    yield documentLoaded;

    let newWidth = content.innerWidth;
    is(originalWidth, newWidth, "Floating scrollbars are presents");

    yield instance._test_notifyOnResize();

    yield nextTick();

    instance.transitionsEnabled = false;

    // Starting from length - 4 because last 3 items are not presets : separator, addbutton and removebutton
    for (let c = instance.menulist.firstChild.childNodes.length - 4; c >= 0; c--) {
      let item = instance.menulist.firstChild.childNodes[c];
      let [width, height] = extractSizeFromString(item.getAttribute("label"));
      let onContentResize = once(mgr, "contentResize");
      instance.menulist.selectedIndex = c;
      yield onContentResize;
      is(content.innerWidth, width, "preset " + c + ": dimension valid (width)");
      is(content.innerHeight, height, "preset " + c + ": dimension valid (height)");
    }

    // test custom

    instance.setSize(100, 100);

    yield once(mgr, "contentResize");

    let initialWidth = content.innerWidth;
    let initialHeight = content.innerHeight;

    is(initialWidth, 100, "Width reset to 100");
    is(initialHeight, 100, "Height reset to 100");

    let x = 2, y = 2;
    EventUtils.synthesizeMouse(instance.resizer, x, y, {type: "mousedown"}, window);
    x += 20; y += 10;
    EventUtils.synthesizeMouse(instance.resizer, x, y, {type: "mousemove"}, window);
    EventUtils.synthesizeMouse(instance.resizer, x, y, {type: "mouseup"}, window);

    yield once(mgr, "contentResize");

    let expectedWidth = initialWidth + 20;
    let expectedHeight = initialHeight + 10;
    info("initial width: " + initialWidth);
    info("initial height: " + initialHeight);
    is(content.innerWidth, expectedWidth, "Size correctly updated (width).");
    is(content.innerHeight, expectedHeight, "Size correctly updated (height).");

    is(instance.menulist.selectedIndex, -1, "Custom menuitem cannot be selected");
    let label = instance.menulist.firstChild.firstChild.getAttribute("label");
    let value = instance.menulist.value;
    isnot(label, value, "Label from the menulist item is different than the value of the menulist")
    let [width, height] = extractSizeFromString(label);
    is(width, expectedWidth, "Label updated (width).");
    is(height, expectedHeight, "Label updated (height).");
    [width, height] = extractSizeFromString(value);
    is(width, expectedWidth, "Value updated (width).");
    is(height, expectedHeight, "Value updated (height).");

    // With "shift" key pressed

    instance.setSize(100, 100);

    yield once(mgr, "contentResize");

    initialWidth = content.innerWidth;
    initialHeight = content.innerHeight;

    x = 2; y = 2;
    EventUtils.synthesizeMouse(instance.resizer, x, y, {type: "mousedown"}, window);
    x += 23; y += 13;
    EventUtils.synthesizeMouse(instance.resizer, x, y, {type: "mousemove", shiftKey: true}, window);
    EventUtils.synthesizeMouse(instance.resizer, x, y, {type: "mouseup"}, window);

    yield once(mgr, "contentResize");

    expectedWidth = initialWidth + 20;
    expectedHeight = initialHeight + 10;
    is(content.innerWidth, expectedWidth, "with shift: Size correctly updated (width).");
    is(content.innerHeight, expectedHeight, "with shift: Size correctly updated (height).");
    is(instance.menulist.selectedIndex, -1, "with shift: Custom menuitem cannot be selected");
    label = instance.menulist.firstChild.firstChild.getAttribute("label");
    value = instance.menulist.value;
    isnot(label, value, "Label from the menulist item is different than the value of the menulist");
    [width, height] = extractSizeFromString(label);
    is(width, expectedWidth, "Label updated (width).");
    is(height, expectedHeight, "Label updated (height).");
    [width, height] = extractSizeFromString(value);
    is(width, expectedWidth, "Value updated (width).");
    is(height, expectedHeight, "Value updated (height).");


    // With "ctrl" key pressed

    instance.setSize(100, 100);

    yield once(mgr, "contentResize");

    initialWidth = content.innerWidth;
    initialHeight = content.innerHeight;

    x = 2; y = 2;
    EventUtils.synthesizeMouse(instance.resizer, x, y, {type: "mousedown"}, window);
    x += 60; y += 30;
    EventUtils.synthesizeMouse(instance.resizer, x, y, {type: "mousemove", ctrlKey: true}, window);
    EventUtils.synthesizeMouse(instance.resizer, x, y, {type: "mouseup"}, window);

    yield once(mgr, "contentResize");

    expectedWidth = initialWidth + 10;
    expectedHeight = initialHeight + 5;
    is(content.innerWidth, expectedWidth, "with ctrl: Size correctly updated (width).");
    is(content.innerHeight, expectedHeight, "with ctrl: Size correctly updated (height).");
    is(instance.menulist.selectedIndex, -1, "with ctrl: Custom menuitem cannot be selected");
    label = instance.menulist.firstChild.firstChild.getAttribute("label");
    value = instance.menulist.value;
    isnot(label, value, "Label from the menulist item is different than the value of the menulist");
    [width, height] = extractSizeFromString(label);
    is(width, expectedWidth, "Label updated (width).");
    is(height, expectedHeight, "Label updated (height).");
    [width, height] = extractSizeFromString(value);
    is(width, expectedWidth, "Value updated (width).");
    is(height, expectedHeight, "Value updated (height).");


    // Test custom input

    initialWidth = content.innerWidth;
    initialHeight = content.innerHeight;
    expectedWidth = initialWidth - 20;
    expectedHeight = initialHeight - 10;
    let index = instance.menulist.selectedIndex;

    let userInput = expectedWidth + " x " + expectedHeight;

    instance.menulist.inputField.value = "";
    instance.menulist.focus();
    processStringAsKey(userInput);

    // While typing, the size should not change
    is(content.innerWidth, initialWidth, "Size hasn't changed (width).");
    is(content.innerHeight, initialHeight, "Size hasn't changed (height).");

    // Only the `change` event must change the size
    EventUtils.synthesizeKey("VK_RETURN", {});

    yield once(mgr, "contentResize");

    is(content.innerWidth, expectedWidth, "Size correctly updated (width).");
    is(content.innerHeight, expectedHeight, "Size correctly updated (height).");
    is(instance.menulist.selectedIndex, -1, "Custom menuitem cannot be selected");
    label = instance.menulist.firstChild.firstChild.getAttribute("label");
    value = instance.menulist.value;
    isnot(label, value, "Label from the menulist item is different than the value of the menulist");
    [width, height] = extractSizeFromString(label);
    is(width, expectedWidth, "Label updated (width).");
    is(height, expectedHeight, "Label updated (height).");
    [width, height] = extractSizeFromString(value);
    is(width, expectedWidth, "Value updated (width).");
    is(height, expectedHeight, "Value updated (height).");


    // Invalid input


    initialWidth = content.innerWidth;
    initialHeight = content.innerHeight;
    index = instance.menulist.selectedIndex;
    let expectedValue = initialWidth + "\u00D7" + initialHeight;
    let expectedLabel = instance.menulist.firstChild.firstChild.getAttribute("label");

    userInput = "I'm wrong";

    instance.menulist.inputField.value = "";
    instance.menulist.focus();
    processStringAsKey(userInput);
    EventUtils.synthesizeKey("VK_RETURN", {});

    is(content.innerWidth, initialWidth, "Size hasn't changed (width).");
    is(content.innerHeight, initialHeight, "Size hasn't changed (height).");
    is(instance.menulist.selectedIndex, index, "Selected item hasn't changed.");
    is(instance.menulist.value, expectedValue, "Value has been reset")
    label = instance.menulist.firstChild.firstChild.getAttribute("label");
    is(label, expectedLabel, "Custom menuitem's label hasn't changed");


    // Rotate

    initialWidth = content.innerWidth;
    initialHeight = content.innerHeight;

    info("rotate");
    instance.rotate();

    yield once(mgr, "contentResize");

    is(content.innerWidth, initialHeight, "The width is now the height.");
    is(content.innerHeight, initialWidth, "The height is now the width.");
    [width, height] = extractSizeFromString(instance.menulist.firstChild.firstChild.getAttribute("label"));
    is(width, initialHeight, "Label updated (width).");
    is(height, initialWidth, "Label updated (height).");

    let widthBeforeClose = content.innerWidth;
    let heightBeforeClose = content.innerHeight;

    // Restart

    mgr.toggle(window, gBrowser.selectedTab);

    yield once(mgr, "off");

    mgr.toggle(window, gBrowser.selectedTab);

    yield once(mgr, "on");

    container = gBrowser.getBrowserContainer();
    is(container.getAttribute("responsivemode"), "true", "In responsive mode.");

    // Menus are correctly updated?

    is(document.getElementById("Tools:ResponsiveUI").getAttribute("checked"), "true", "menus checked");

    is(content.innerWidth, widthBeforeClose, "width restored.");
    is(content.innerHeight, heightBeforeClose, "height restored.");

    // Screenshot


    let isWinXP = navigator.userAgent.indexOf("Windows NT 5.1") != -1;
    if (!isWinXP) {
      info("screenshot");
      instance.screenshot("responsiveui");
      let FileUtils = (Cu.import("resource://gre/modules/FileUtils.jsm", {})).FileUtils;

      while(true) {
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

    mgr.toggle(window, gBrowser.selectedTab);

    yield once(mgr, "off");

    // Menus are correctly updated?
    is(document.getElementById("Tools:ResponsiveUI").getAttribute("checked"), "false", "menu unchecked");

    delete instance;
    gBrowser.removeCurrentTab();
    finish();

  });
}
