/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test opening and closing the menu panel UI.
 */

// Show and hide the menu panel programmatically without an event (like UITour.jsm would)
add_task(function*() {
  let shownPromise = promisePanelShown(window);
  PanelUI.show();
  yield shownPromise;

  is(PanelUI.panel.getAttribute("panelopen"), "true", "Check that panel has panelopen attribute");
  is(PanelUI.panel.state, "open", "Check that panel state is 'open'");

  let hiddenPromise = promisePanelHidden(window);
  PanelUI.hide();
  yield hiddenPromise;

  ok(!PanelUI.panel.hasAttribute("panelopen"), "Check that panel doesn't have the panelopen attribute");
  is(PanelUI.panel.state, "closed", "Check that panel state is 'closed'");
});

// Toggle the menu panel open and closed
add_task(function*() {
  let shownPromise = promisePanelShown(window);
  PanelUI.toggle({type: "command"});
  yield shownPromise;

  is(PanelUI.panel.getAttribute("panelopen"), "true", "Check that panel has panelopen attribute");
  is(PanelUI.panel.state, "open", "Check that panel state is 'open'");

  let hiddenPromise = promisePanelHidden(window);
  PanelUI.toggle({type: "command"});
  yield hiddenPromise;

  ok(!PanelUI.panel.hasAttribute("panelopen"), "Check that panel doesn't have the panelopen attribute");
  is(PanelUI.panel.state, "closed", "Check that panel state is 'closed'");
});
