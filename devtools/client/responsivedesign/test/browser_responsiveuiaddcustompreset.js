/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function*() {
  let tab = yield addTab("data:text/html;charset=utf8,Test RDM custom presets");

  let {rdm} = yield openRDM(tab);

  let oldPrompt = Services.prompt;
  Services.prompt = {
    value: "",
    returnBool: true,
    prompt: function(parent, dialogTitle, text, value, checkMsg, checkState) {
      value.value = this.value;
      return this.returnBool;
    }
  };

  registerCleanupFunction(() => {
    Services.prompt = oldPrompt;
  });

  // Is it open?
  let container = gBrowser.getBrowserContainer();
  is(container.getAttribute("responsivemode"), "true",
     "Should be in responsive mode.");

  ok(rdm, "RDM instance should be attached to the tab.");

  yield rdm._test_notifyOnResize();

  // Tries to add a custom preset and cancel the prompt
  let idx = rdm.menulist.selectedIndex;
  let presetCount = rdm.presets.length;

  Services.prompt.value = "";
  Services.prompt.returnBool = false;
  rdm.addbutton.doCommand();

  is(idx, rdm.menulist.selectedIndex,
     "selected item shouldn't change after add preset and cancel");
  is(presetCount, rdm.presets.length,
     "number of presets shouldn't change after add preset and cancel");

  // Adds the custom preset with "Testing preset"
  Services.prompt.value = "Testing preset";
  Services.prompt.returnBool = true;

  let customHeight = 123, customWidth = 456;
  rdm.startResizing({});
  rdm.setSize(customWidth, customHeight);
  rdm.stopResizing({});

  rdm.addbutton.doCommand();

  // Force document reflow to avoid intermittent failures.
  info("document height " + document.height);

  yield closeRDM(rdm);

  // We're still in the loop of initializing the responsive mode.
  // Let's wait next loop to stop it.
  yield waitForTick();

  ({rdm} = yield openRDM(tab));
  is(container.getAttribute("responsivemode"), "true",
     "Should be in responsive mode.");

  let presetLabel = "456" + "\u00D7" + "123 (Testing preset)";
  let customPresetIndex = getPresetIndex(rdm, presetLabel);
  info(customPresetIndex);
  ok(customPresetIndex >= 0, "(idx = " + customPresetIndex + ") should be the" +
                             " previously added preset in the list of items");

  let resizePromise = rdm._test_notifyOnResize();
  rdm.menulist.selectedIndex = customPresetIndex;
  yield resizePromise;

  let browser = gBrowser.selectedBrowser;
  let props = yield ContentTask.spawn(browser, {}, function*() {
    let {innerWidth, innerHeight} = content;
    return {innerWidth, innerHeight};
  });

  is(props.innerWidth, 456, "Selecting preset should change the width");
  is(props.innerHeight, 123, "Selecting preset should change the height");

  rdm.removebutton.doCommand();

  rdm.menulist.selectedIndex = 2;
  let deletedPresetA = rdm.menulist.selectedItem.getAttribute("label");
  rdm.removebutton.doCommand();

  rdm.menulist.selectedIndex = 2;
  let deletedPresetB = rdm.menulist.selectedItem.getAttribute("label");
  rdm.removebutton.doCommand();

  yield closeRDM(rdm);
  yield waitForTick();
  ({rdm} = yield openRDM(tab));

  customPresetIndex = getPresetIndex(rdm, deletedPresetA);
  is(customPresetIndex, -1,
     "Deleted preset " + deletedPresetA + " should not be in the list anymore");

  customPresetIndex = getPresetIndex(rdm, deletedPresetB);
  is(customPresetIndex, -1,
     "Deleted preset " + deletedPresetB + " should not be in the list anymore");

  yield closeRDM(rdm);
});

function getPresetIndex(rdm, presetLabel) {
  function testOnePreset(c) {
    if (c == 0) {
      return -1;
    }
    rdm.menulist.selectedIndex = c;

    let item = rdm.menulist.firstChild.childNodes[c];
    if (item.getAttribute("label") === presetLabel) {
      return c;
    }
    return testOnePreset(c - 1);
  }
  return testOnePreset(rdm.menulist.firstChild.childNodes.length - 4);
}
