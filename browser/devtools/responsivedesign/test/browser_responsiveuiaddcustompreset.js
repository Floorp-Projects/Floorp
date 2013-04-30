/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let instance, deletedPresetA, deletedPresetB, oldPrompt;
  let mgr = ResponsiveUI.ResponsiveUIManager;

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(startTest, content);
  }, true);

  content.location = "data:text/html;charset=utf8,test custom presets in responsive mode";

  // This test uses executeSoon() when responsive mode is initialized and when
  // it is destroyed such that we get out of the init/destroy loops. If we try
  // to init/destroy immediately, without waiting for the next loop, we get
  // intermittent test failures.

  function startTest() {
    // Mocking prompt
    oldPrompt = Services.prompt;
    Services.prompt = {
      value: "",
      returnBool: true,
      prompt: function(aParent, aDialogTitle, aText, aValue, aCheckMsg, aCheckState) {
        aValue.value = this.value;
        return this.returnBool;
      }
    };

    registerCleanupFunction(() => Services.prompt = oldPrompt);

    info("test started, waiting for responsive mode to activate");

    document.getElementById("Tools:ResponsiveUI").removeAttribute("disabled");
    mgr.once("on", onUIOpen);
    synthesizeKeyFromKeyTag("key_responsiveUI");
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
    // Tries to add a custom preset and cancel the prompt
    let idx = instance.menulist.selectedIndex;
    let presetCount = instance.presets.length;

    Services.prompt.value = "";
    Services.prompt.returnBool = false;
    instance.addbutton.doCommand();

    is(idx, instance.menulist.selectedIndex, "selected item didn't change after add preset and cancel");
    is(presetCount, instance.presets.length, "number of presets didn't change after add preset and cancel");

    let customHeight = 123, customWidth = 456;
    instance.setSize(customWidth, customHeight);

    // Adds the custom preset with "Testing preset"
    Services.prompt.value = "Testing preset";
    Services.prompt.returnBool = true;
    instance.addbutton.doCommand();

    instance.menulist.selectedIndex = 1;

    info("waiting for responsive mode to turn off");
    mgr.once("off", restart);

    // Force document reflow to avoid intermittent failures.
    info("document height " + document.height);

    // We're still in the loop of initializing the responsive mode.
    // Let's wait next loop to stop it.
    executeSoon(function() {
      EventUtils.synthesizeKey("VK_ESCAPE", {});
    });
  }

  function restart() {
    info("Restarting Responsive Mode");
    mgr.once("on", function() {
      let container = gBrowser.getBrowserContainer();
      is(container.getAttribute("responsivemode"), "true", "In responsive mode.");

      instance = gBrowser.selectedTab.__responsiveUI;

      testCustomPresetInList();
    });

    // We're still in the loop of destroying the responsive mode.
    // Let's wait next loop to start it.
    executeSoon(function() {
      synthesizeKeyFromKeyTag("key_responsiveUI");
    });
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

    info("waiting for responsive mode to turn off");
    mgr.once("off", restartAgain);

    // We're still in the loop of initializing the responsive mode.
    // Let's wait next loop to stop it.
    executeSoon(() => EventUtils.synthesizeKey("VK_ESCAPE", {}));
  }

  function restartAgain() {
    info("waiting for responsive mode to turn on");
    mgr.once("on", () => {
      instance = gBrowser.selectedTab.__responsiveUI;
      testCustomPresetsNotInListAnymore();
    });

    // We're still in the loop of destroying the responsive mode.
    // Let's wait next loop to start it.
    executeSoon(() => synthesizeKeyFromKeyTag("key_responsiveUI"));
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

    let name = null;

    if (key.getAttribute("keycode"))
      name = key.getAttribute("keycode");
    else if (key.getAttribute("key"))
      name = key.getAttribute("key");

    isnot(name, null, "Successfully retrieved keycode/key");

    key.doCommand();
  }
}
