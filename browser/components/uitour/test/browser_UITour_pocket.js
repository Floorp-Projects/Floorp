/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gTestTab;
var gContentAPI;

add_task(setup_UITourTest);

add_UITour_task(async function test_menu_show() {
  let panel = BrowserPageActions.activatedActionPanelNode;
  Assert.ok(
    !panel || panel.state == "closed",
    "Pocket panel should initially be closed"
  );
  gContentAPI.showMenu("pocket");

  // The panel gets created dynamically.
  panel = null;
  await waitForConditionPromise(() => {
    panel = BrowserPageActions.activatedActionPanelNode;
    return panel && panel.state == "open";
  }, "Menu should be visible after showMenu()");

  Assert.ok(
    !panel.hasAttribute("noautohide"),
    "@noautohide shouldn't be on the pocket panel"
  );

  panel.hidePopup();
  await new Promise(resolve => {
    panel = BrowserPageActions.activatedActionPanelNode;
    if (!panel || panel.state == "closed") {
      resolve();
    }
  });
});
