"use strict";

const BUTTONID = "test-seenwidget-post-reset";

add_task(async function () {
  CustomizableUI.createWidget({
    id: BUTTONID,
    label: "Test widget seen post reset",
    defaultArea: CustomizableUI.AREA_NAVBAR,
  });

  const kPrefCustomizationState = "browser.uiCustomization.state";
  ok(
    CustomizableUI.getTestOnlyInternalProp("gSeenWidgets").has(BUTTONID),
    "Widget should be seen after createWidget is called."
  );
  CustomizableUI.reset();
  ok(
    CustomizableUI.getTestOnlyInternalProp("gSeenWidgets").has(BUTTONID),
    "Widget should still be seen after reset."
  );
  CustomizableUI.addWidgetToArea(BUTTONID, CustomizableUI.AREA_NAVBAR);
  gCustomizeMode.removeFromArea(document.getElementById(BUTTONID));
  let hasUserValue = Services.prefs.prefHasUserValue(kPrefCustomizationState);
  ok(hasUserValue, "Pref should be set right now.");
  if (hasUserValue) {
    let seenArray = JSON.parse(
      Services.prefs.getCharPref(kPrefCustomizationState)
    ).seen;
    isnot(
      seenArray.indexOf(BUTTONID),
      -1,
      "Widget should be in saved 'seen' list."
    );
  }
});

registerCleanupFunction(function () {
  CustomizableUI.destroyWidget(BUTTONID);
  CustomizableUI.reset();
});
