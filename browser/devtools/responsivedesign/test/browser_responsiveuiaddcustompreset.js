/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();
  SimpleTest.requestCompleteLog();

  let instance, deletedPresetA, deletedPresetB, oldPrompt;

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

  Task.spawn(function() {

    yield addTab("data:text/html;charset=utf8,test custom presets in responsive mode");

    let mgr = ResponsiveUI.ResponsiveUIManager;

    synthesizeKeyFromKeyTag("key_responsiveUI");

    yield once(mgr, "on");

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

    // Is it open?
    let container = gBrowser.getBrowserContainer();
    is(container.getAttribute("responsivemode"), "true", "In responsive mode.");

    instance = mgr.getResponsiveUIForTab(gBrowser.selectedTab);
    ok(instance, "instance of the module is attached to the tab.");

    instance.transitionsEnabled = false;

    yield instance._test_notifyOnResize();

    // Tries to add a custom preset and cancel the prompt
    let idx = instance.menulist.selectedIndex;
    let presetCount = instance.presets.length;

    Services.prompt.value = "";
    Services.prompt.returnBool = false;
    instance.addbutton.doCommand();

    is(idx, instance.menulist.selectedIndex, "selected item didn't change after add preset and cancel");
    is(presetCount, instance.presets.length, "number of presets didn't change after add preset and cancel");

    // Adds the custom preset with "Testing preset"
    Services.prompt.value = "Testing preset";
    Services.prompt.returnBool = true;

    let customHeight = 123, customWidth = 456;
    instance.startResizing({});
    instance.setSize(customWidth, customHeight);
    instance.stopResizing({});

    instance.addbutton.doCommand();

    // Force document reflow to avoid intermittent failures.
    info("document height " + document.height);

    instance.close();

    info("waiting for responsive mode to turn off");
    yield once(mgr, "off");

    // We're still in the loop of initializing the responsive mode.
    // Let's wait next loop to stop it.
    yield nextTick();

    synthesizeKeyFromKeyTag("key_responsiveUI");

    yield once(mgr, "on");

    is(container.getAttribute("responsivemode"), "true", "In responsive mode.");

    instance = mgr.getResponsiveUIForTab(gBrowser.selectedTab);

    let customPresetIndex = getPresetIndex("456" + "\u00D7" + "123 (Testing preset)");
    info(customPresetIndex);
    ok(customPresetIndex >= 0, "is the previously added preset (idx = " + customPresetIndex + ") in the list of items");

    instance.menulist.selectedIndex = customPresetIndex;

    is(content.innerWidth, 456, "add preset, and selected in the list, dimension valid (width)");
    is(content.innerHeight, 123, "add preset, and selected in the list, dimension valid (height)");

    instance.removebutton.doCommand();

    instance.menulist.selectedIndex = 2;
    deletedPresetA = instance.menulist.selectedItem.getAttribute("label");
    instance.removebutton.doCommand();

    instance.menulist.selectedIndex = 2;
    deletedPresetB = instance.menulist.selectedItem.getAttribute("label");
    instance.removebutton.doCommand();

    yield nextTick();
    instance.close();
    yield once(mgr, "off");

    synthesizeKeyFromKeyTag("key_responsiveUI");

    info("waiting for responsive mode to turn on");
    yield once(mgr, "on");

    instance = mgr.getResponsiveUIForTab(gBrowser.selectedTab);

    customPresetIndex = getPresetIndex(deletedPresetA);
    is(customPresetIndex, -1, "deleted preset " + deletedPresetA + " is not in the list anymore");

    customPresetIndex = getPresetIndex(deletedPresetB);
    is(customPresetIndex, -1, "deleted preset " + deletedPresetB + " is not in the list anymore");

    yield nextTick();

    gBrowser.removeCurrentTab();
    finish();
  });
}
