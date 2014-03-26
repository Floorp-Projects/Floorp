/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function() {
  info("Check Tab Groups button functionality");
  let deferred = Promise.defer();
  let timeout = null;

  // add the Tab View button to the panel
  CustomizableUI.addWidgetToArea("tabview-button", CustomizableUI.AREA_PANEL);

  window.addEventListener("tabviewhidden", function tabViewHidden() {
    clearTimeout(timeout);
    window.removeEventListener("tabviewhidden", tabViewHidden, false);
    ok(true, "Tab View is closed");
    deferred.resolve();
  }, false);

  window.addEventListener("tabviewshown", function tabViewShown() {
    window.removeEventListener("tabviewshown", tabViewShown, false);
    ok(true, "Tab Groups are loaded");
    // close tabs view
    window.TabView.hide();
    timeout = setTimeout(() => {
      window.removeEventListener("tabviewhidden", tabViewHidden, false);
      deferred.reject("Tabs view wasn't hidden within 20000ms");
    }, 20000);
  }, false);

  // check the button's functionality
  yield PanelUI.show();

  let tabViewButton = document.getElementById("tabview-button");
  ok(tabViewButton, "Tab Groups button was added to the Panel Menu");
  tabViewButton.click();

  yield deferred.promise;

  ok(!isPanelUIOpen(), "The panel is closed");

  if(isPanelUIOpen()) {
    let panelHidePromise = promisePanelHidden(window);
    PanelUI.hide();
    yield panelHidePromise;
  }
});

add_task(function asyncCleanup() {
  // reset the panel to the default state
  yield resetCustomization();
  ok(CustomizableUI.inDefaultState, "UI is in default state again.");
});
