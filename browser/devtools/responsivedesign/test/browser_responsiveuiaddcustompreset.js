/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let instance, deletedPresetA, deletedPresetB, oldPrompt;

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(startTest, content);
  }, true);

  content.location = "data:text/html,foo";

  function startTest() {
    // Mocking prompt
    oldPrompt = Services.prompt;
    Services.prompt = {
      prompt: function(aParent, aDialogTitle, aText, aValue, aCheckMsg, aCheckState) {
        aValue.value = "Testing preset";
      }
    };

    document.getElementById("Tools:ResponsiveUI").removeAttribute("disabled");
    synthesizeKeyFromKeyTag("key_responsiveUI");
    executeSoon(onUIOpen);
  }

  function onUIOpen() {
    // Is it open?
    let container = gBrowser.getBrowserContainer();
    is(container.getAttribute("responsivemode"), "true", "In responsive mode.");

    instance = gBrowser.selectedTab.__responsiveUI;
    ok(instance, "instance of the module is attached to the tab.");

    instance.transitionsEnabled = false;

    testAddCustomPreset();
  }

  function testAddCustomPreset() {
    let customHeight = 123, customWidth = 456;
    instance.setSize(customWidth, customHeight);

    // Adds the custom preset with "Testing preset" as label (look at mock upper)
    instance.addbutton.doCommand();

    instance.menulist.selectedIndex = 1;

    EventUtils.synthesizeKey("VK_ESCAPE", {});
    executeSoon(restart);
  }

  function restart() {
    synthesizeKeyFromKeyTag("key_responsiveUI");

    let container = gBrowser.getBrowserContainer();
    is(container.getAttribute("responsivemode"), "true", "In responsive mode.");

    instance = gBrowser.selectedTab.__responsiveUI;

    testCustomPresetInList();
  }

  function testCustomPresetInList() {
    let customPresetIndex = getPresetIndex("456x123 (Testing preset)");
    ok(customPresetIndex >= 0, "is the previously added preset (idx = " + customPresetIndex + ") in the list of items");

    instance.menulist.selectedIndex = customPresetIndex;

    is(content.innerWidth, 456, "add preset, and selected in the list, dimension valid (width)");
    is(content.innerHeight, 123, "add preset, and selected in the list, dimension valid (height)");

    testDeleteCustomPresets();
  }

  function testDeleteCustomPresets() {
    instance.removebutton.doCommand();

    instance.menulist.selectedIndex = 2;
    deletedPresetA = instance.menulist.selectedItem.getAttribute("label");
    instance.removebutton.doCommand();

    instance.menulist.selectedIndex = 2;
    deletedPresetB = instance.menulist.selectedItem.getAttribute("label");
    instance.removebutton.doCommand();

    EventUtils.synthesizeKey("VK_ESCAPE", {});
    executeSoon(restartAgain);
  }

  function restartAgain() {
    synthesizeKeyFromKeyTag("key_responsiveUI");
    instance = gBrowser.selectedTab.__responsiveUI;
    executeSoon(testCustomPresetsNotInListAnymore);
  }

  function testCustomPresetsNotInListAnymore() {
    let customPresetIndex = getPresetIndex(deletedPresetA);
    is(customPresetIndex, -1, "deleted preset " + deletedPresetA + " is not in the list anymore");

    customPresetIndex = getPresetIndex(deletedPresetB);
    is(customPresetIndex, -1, "deleted preset " + deletedPresetB + " is not in the list anymore");

    executeSoon(finishUp);
  }

  function finishUp() {
    delete instance;
    gBrowser.removeCurrentTab();

    Services.prompt = oldPrompt;

    finish();
  }

  function getPresetIndex(presetLabel) {
    function testOnePreset(c) {
      if (c == 0) {
        return -1;
      }
      instance.menulist.selectedIndex = c;

      let item = instance.menulist.firstChild.childNodes[c];
      if (item.getAttribute("label") === presetLabel) {
        return c;
      } else {
        return testOnePreset(c - 1);
      }
    }
    return testOnePreset(instance.menulist.firstChild.childNodes.length - 4);
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
