/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PAGE = "http://mochi.test:8888/browser/browser/components/customizableui/test/support/test_967000_charEncoding_page.html";

let newTab;
let initialLocation = gBrowser.currentURI.spec;

add_task(function() {
  info("Check Character Encoding button functionality");

  // add the Character Encoding button to the panel
  CustomizableUI.addWidgetToArea("characterencoding-button",
                                  CustomizableUI.AREA_PANEL);

  // check the button's functionality
  yield PanelUI.show();

  let charEncodingButton = document.getElementById("characterencoding-button");
  ok(charEncodingButton, "The Character Encoding button was added to the Panel Menu");
  is(charEncodingButton.getAttribute("disabled"), "true",
     "The Character encoding button is initially disabled");

  let panelHidePromise = promisePanelHidden(window);
  PanelUI.hide();
  yield panelHidePromise;

  newTab = gBrowser.selectedTab;
  yield promiseTabLoadEvent(newTab, TEST_PAGE)

  yield PanelUI.show();
  ok(!charEncodingButton.hasAttribute("disabled"), "The Character encoding button gets enabled");
  charEncodingButton.click();

  let characterEncodingView = document.getElementById("PanelUI-characterEncodingView");
  ok(characterEncodingView.hasAttribute("current"), "The Character encoding panel is displayed");

  let pinnedEncodings = document.getElementById("PanelUI-characterEncodingView-pinned");
  let charsetsList = document.getElementById("PanelUI-characterEncodingView-charsets");
  ok(pinnedEncodings, "Pinned charsets are available");
  ok(charsetsList, "Charsets list is available");

  let checkedButtons = characterEncodingView.querySelectorAll("toolbarbutton[checked='true']");
  is(checkedButtons.length, 2, "There should be 2 checked items (1 charset, 1 detector).");
  is(checkedButtons[0].getAttribute("label"), "Unicode", "The unicode encoding is correctly selected");
  is(characterEncodingView.querySelectorAll("#PanelUI-characterEncodingView-autodetect toolbarbutton[checked='true']").length,
     1,
     "There should be 1 checked detector.");

  panelHidePromise = promisePanelHidden(window);
  PanelUI.hide();
  yield panelHidePromise;
});

add_task(function asyncCleanup() {
  // reset the panel to the default state
  yield resetCustomization();
  ok(CustomizableUI.inDefaultState, "The UI is in default state again.");

  // restore the initial location
  gBrowser.addTab(initialLocation);
  gBrowser.removeTab(newTab);
});
