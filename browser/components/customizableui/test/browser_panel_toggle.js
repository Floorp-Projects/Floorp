/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test opening and closing the menu panel UI.
 */

// Show and hide the menu panel programmatically without an event (like UITour.sys.mjs would)
add_task(async function () {
  await gCUITestUtils.openMainMenu();

  is(
    PanelUI.panel.getAttribute("panelopen"),
    "true",
    "Check that panel has panelopen attribute"
  );
  is(PanelUI.panel.state, "open", "Check that panel state is 'open'");

  await gCUITestUtils.hideMainMenu();

  ok(
    !PanelUI.panel.hasAttribute("panelopen"),
    "Check that panel doesn't have the panelopen attribute"
  );
  is(PanelUI.panel.state, "closed", "Check that panel state is 'closed'");
});

// Toggle the menu panel open and closed
add_task(async function () {
  await gCUITestUtils.openPanelMultiView(PanelUI.panel, PanelUI.mainView, () =>
    PanelUI.toggle({ type: "command" })
  );

  is(
    PanelUI.panel.getAttribute("panelopen"),
    "true",
    "Check that panel has panelopen attribute"
  );
  is(PanelUI.panel.state, "open", "Check that panel state is 'open'");

  await gCUITestUtils.hidePanelMultiView(PanelUI.panel, () =>
    PanelUI.toggle({ type: "command" })
  );

  ok(
    !PanelUI.panel.hasAttribute("panelopen"),
    "Check that panel doesn't have the panelopen attribute"
  );
  is(PanelUI.panel.state, "closed", "Check that panel state is 'closed'");
});
