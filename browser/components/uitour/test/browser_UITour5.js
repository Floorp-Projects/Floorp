"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

add_task(setup_UITourTest);

add_UITour_task(async function test_highlight_library_and_show_library_subview() {
  let highlight = document.getElementById("UITourHighlight");
  is_element_hidden(highlight, "Highlight should initially be hidden");

  // Test highlighting the library button
  let appMenu = PanelUI.panel;
  let appMenuShownPromise = promisePanelElementShown(window, appMenu);
  let highlightVisiblePromise = elementVisiblePromise(highlight, "Should show highlight");
  gContentAPI.showHighlight("library");
  await appMenuShownPromise;
  await highlightVisiblePromise;
  is(appMenu.state, "open", "Should open the app menu to highlight the library button");
  is(getShowHighlightTargetName(), "library", "Should highlight the library button on the app menu");

  // Click the library button to show the subview
  let ViewShownPromise = new Promise(resolve => {
    appMenu.addEventListener("ViewShown", resolve, { once: true });
  });
  let highlightHiddenPromise = elementHiddenPromise(highlight, "Should hide highlight");
  let libraryBtn = document.getElementById("appMenu-library-button");
  libraryBtn.dispatchEvent(new Event("command"));
  await highlightHiddenPromise;
  await ViewShownPromise;
  is(PanelUI.multiView.current.id, "appMenu-libraryView", "Should show the library subview");
  is(appMenu.state, "open", "Should still open the app menu for the library subview");

  // Clean up
  let appMenuHiddenPromise = promisePanelElementHidden(window, appMenu);
  gContentAPI.hideMenu("appMenu");
  await appMenuHiddenPromise;
  is(appMenu.state, "closed", "Should close the app menu");
});
