/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function* () {
  let tab = yield addTab("data:text/html;charset=utf8,Test RDM custom presets");

  let { rdm, manager } = yield openRDM(tab);

  let oldPrompt = Services.prompt;
  Services.prompt = {
    value: "",
    returnBool: true,
    prompt: function (parent, dialogTitle, text, value, checkMsg, checkState) {
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

  let resized = once(manager, "contentResize");
  let customHeight = 123, customWidth = 456;
  rdm.startResizing({});
  rdm.setSize(customWidth, customHeight);
  rdm.stopResizing({});

  rdm.addbutton.doCommand();
  yield resized;

  yield closeRDM(rdm);

  ({rdm} = yield openRDM(tab));
  is(container.getAttribute("responsivemode"), "true",
     "Should be in responsive mode.");

  let presetLabel = "456" + "\u00D7" + "123 (Testing preset)";
  let customPresetIndex = yield getPresetIndex(rdm, manager, presetLabel);
  ok(customPresetIndex >= 0, "(idx = " + customPresetIndex + ") should be the" +
                             " previously added preset in the list of items");

  yield setPresetIndex(rdm, manager, customPresetIndex);

  let browser = gBrowser.selectedBrowser;
  yield ContentTask.spawn(browser, null, function* () {
    let {innerWidth, innerHeight} = content;
    Assert.equal(innerWidth, 456, "Selecting preset should change the width");
    Assert.equal(innerHeight, 123, "Selecting preset should change the height");
  });

  info(`menulist count: ${rdm.menulist.itemCount}`);

  rdm.removebutton.doCommand();

  yield setPresetIndex(rdm, manager, 2);
  let deletedPresetA = rdm.menulist.selectedItem.getAttribute("label");
  rdm.removebutton.doCommand();

  yield setPresetIndex(rdm, manager, 2);
  let deletedPresetB = rdm.menulist.selectedItem.getAttribute("label");
  rdm.removebutton.doCommand();

  yield closeRDM(rdm);
  ({rdm} = yield openRDM(tab));

  customPresetIndex = yield getPresetIndex(rdm, manager, deletedPresetA);
  is(customPresetIndex, -1,
     "Deleted preset " + deletedPresetA + " should not be in the list anymore");

  customPresetIndex = yield getPresetIndex(rdm, manager, deletedPresetB);
  is(customPresetIndex, -1,
     "Deleted preset " + deletedPresetB + " should not be in the list anymore");

  yield closeRDM(rdm);
});

var getPresetIndex = Task.async(function* (rdm, manager, presetLabel) {
  var testOnePreset = Task.async(function* (c) {
    if (c == 0) {
      return -1;
    }
    yield setPresetIndex(rdm, manager, c);

    let item = rdm.menulist.firstChild.childNodes[c];
    if (item.getAttribute("label") === presetLabel) {
      return c;
    }
    return testOnePreset(c - 1);
  });
  return testOnePreset(rdm.menulist.firstChild.childNodes.length - 4);
});
