/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.photon.structure.enabled", false]]});
  const kNormalLabel = "Character Encoding";
  CustomizableUI.addWidgetToArea("characterencoding-button", CustomizableUI.AREA_NAVBAR);
  let characterEncoding = document.getElementById("characterencoding-button");
  const kOriginalLabel = characterEncoding.getAttribute("label");
  characterEncoding.setAttribute("label", "\u00ad" + kNormalLabel);
  CustomizableUI.addWidgetToArea("characterencoding-button", CustomizableUI.AREA_PANEL);

  await PanelUI.show();

  is(characterEncoding.getAttribute("auto-hyphens"), "off",
     "Hyphens should be disabled if the &shy; character is present in the label");
  let multilineText = document.getAnonymousElementByAttribute(characterEncoding, "class", "toolbarbutton-multiline-text");
  let multilineTextCS = getComputedStyle(multilineText);
  is(multilineTextCS.MozHyphens, "manual", "-moz-hyphens should be set to manual when the &shy; character is present.")

  let hiddenPanelPromise = promisePanelHidden(window);
  PanelUI.toggle();
  await hiddenPanelPromise;

  characterEncoding.setAttribute("label", kNormalLabel);

  await PanelUI.show();

  isnot(characterEncoding.getAttribute("auto-hyphens"), "off",
        "Hyphens should not be disabled if the &shy; character is not present in the label");
  multilineText = document.getAnonymousElementByAttribute(characterEncoding, "class", "toolbarbutton-multiline-text");
  multilineTextCS = getComputedStyle(multilineText);
  is(multilineTextCS.MozHyphens, "auto", "-moz-hyphens should be set to auto by default.")

  hiddenPanelPromise = promisePanelHidden(window);
  PanelUI.toggle();
  await hiddenPanelPromise;

  characterEncoding.setAttribute("label", "\u00ad" + kNormalLabel);
  CustomizableUI.removeWidgetFromArea("characterencoding-button");
  await startCustomizing();

  isnot(characterEncoding.getAttribute("auto-hyphens"), "off",
        "Hyphens should not be disabled when the widget is in the palette");

  gCustomizeMode.addToPanel(characterEncoding);
  is(characterEncoding.getAttribute("auto-hyphens"), "off",
     "Hyphens should be disabled if the &shy; character is present in the label in customization mode");
  multilineText = document.getAnonymousElementByAttribute(characterEncoding, "class", "toolbarbutton-multiline-text");
  multilineTextCS = getComputedStyle(multilineText);
  is(multilineTextCS.MozHyphens, "manual", "-moz-hyphens should be set to manual when the &shy; character is present in customization mode.")

  await endCustomizing();

  CustomizableUI.addWidgetToArea("characterencoding-button", CustomizableUI.AREA_NAVBAR);
  ok(!characterEncoding.hasAttribute("auto-hyphens"),
     "Removing the widget from the panel should remove the auto-hyphens attribute");

  characterEncoding.setAttribute("label", kOriginalLabel);
});

add_task(async function asyncCleanup() {
  await endCustomizing();
  await resetCustomization();
});
