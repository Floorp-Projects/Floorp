"use strict";

const BUTTONID = "test-seenwidget-post-reset";

add_task(function*() {
  let widget = CustomizableUI.createWidget({
    id: BUTTONID,
    label: "Test widget seen post reset",
    defaultArea: CustomizableUI.AREA_NAVBAR
  });

  const kPrefCustomizationState = "browser.uiCustomization.state";
  let bsPass = Cu.import("resource:///modules/CustomizableUI.jsm", {});
  ok(bsPass.gSeenWidgets.has(BUTTONID), "Widget should be seen after createWidget is called.");
  CustomizableUI.reset();
  ok(bsPass.gSeenWidgets.has(BUTTONID), "Widget should still be seen after reset.");
  ok(!Services.prefs.prefHasUserValue(kPrefCustomizationState), "Pref shouldn't be set right now, because that'd break undo.");
  CustomizableUI.addWidgetToArea(BUTTONID, CustomizableUI.AREA_NAVBAR);
  gCustomizeMode.removeFromArea(document.getElementById(BUTTONID));
  let hasUserValue = Services.prefs.prefHasUserValue(kPrefCustomizationState);
  ok(hasUserValue, "Pref should be set right now.");
  if (hasUserValue) {
    let seenArray = JSON.parse(Services.prefs.getCharPref(kPrefCustomizationState)).seen;
    isnot(seenArray.indexOf(BUTTONID), -1, "Widget should be in saved 'seen' list.");
  }
});

registerCleanupFunction(function() {
  CustomizableUI.destroyWidget(BUTTONID);
  CustomizableUI.reset();
});
