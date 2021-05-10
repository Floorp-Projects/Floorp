"use strict";

var gTestTab;
var gContentAPI;

add_task(setup_UITourTest);

add_UITour_task(async function test_highlight_help_and_show_help_subview() {
  let highlight = document.getElementById("UITourHighlight");
  is_element_hidden(highlight, "Highlight should initially be hidden");

  // Test highlighting the library button
  let appMenu = PanelUI.panel;
  let appMenuShownPromise = promisePanelElementShown(window, appMenu);
  let highlightVisiblePromise = elementVisiblePromise(
    highlight,
    "Should show highlight"
  );
  gContentAPI.showHighlight("help");
  await appMenuShownPromise;
  await highlightVisiblePromise;
  is(
    appMenu.state,
    "open",
    "Should open the app menu to highlight the help button"
  );
  is(
    getShowHighlightTargetName(),
    "help",
    "Should highlight the library button on the app menu"
  );

  // Click the library button to show the subview
  let ViewShownPromise = new Promise(resolve => {
    appMenu.addEventListener("ViewShown", resolve, { once: true });
  });
  let highlightHiddenPromise = elementHiddenPromise(
    highlight,
    "Should hide highlight"
  );

  let helpButtonID = PanelUI.protonAppMenuEnabled
    ? "appMenu-help-button2"
    : "appMenu-help-button";
  let helpBtn = document.getElementById(helpButtonID);
  helpBtn.dispatchEvent(new Event("command"));
  await highlightHiddenPromise;
  await ViewShownPromise;
  let helpView = document.getElementById("PanelUI-helpView");
  ok(PanelView.forNode(helpView).active, "Should show the help subview");
  is(
    appMenu.state,
    "open",
    "Should still open the app menu for the help subview"
  );

  // Clean up
  let appMenuHiddenPromise = promisePanelElementHidden(window, appMenu);
  gContentAPI.hideMenu("appMenu");
  await appMenuHiddenPromise;
  is(appMenu.state, "closed", "Should close the app menu");
});
