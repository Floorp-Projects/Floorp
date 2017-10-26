"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

add_task(setup_UITourTest);

add_UITour_task(async function test_highlight_library_icon_in_toolbar() {
  let highlight = document.getElementById("UITourHighlight");
  is_element_hidden(highlight, "Highlight should initially be hidden");

  // Test highlighting the library button
  let highlightVisiblePromise = elementVisiblePromise(highlight, "Should show highlight");
  gContentAPI.showHighlight("library");
  await highlightVisiblePromise;
  UITour.getTarget(window, "library").then((target) => {
    is("library-button", target.node.id, "Should highlight the right target");
  });
});

add_UITour_task(async function test_highlight_addons_icon_in_toolbar() {
  CustomizableUI.addWidgetToArea("add-ons-button", CustomizableUI.AREA_NAVBAR, 0);
  ok(!UITour.availableTargetsCache.has(window),
     "Targets should be evicted from cache after widget change");
  let highlight = document.getElementById("UITourHighlight");
  is_element_hidden(highlight, "Highlight should initially be hidden");

  // Test highlighting the addons button on toolbar
  let highlightVisiblePromise = elementVisiblePromise(highlight, "Should show highlight");
  gContentAPI.showHighlight("addons");
  await highlightVisiblePromise;
  UITour.getTarget(window, "addons").then((target) => {
    is("add-ons-button", target.node.id, "Should highlight the right target");
    CustomizableUI.removeWidgetFromArea("add-ons-button");
  });
});

add_UITour_task(async function test_highlight_library_and_show_library_subview() {
  CustomizableUI.removeWidgetFromArea("library-button");

  ok(!UITour.availableTargetsCache.has(window),
     "Targets should be evicted from cache after widget change");
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
  CustomizableUI.addWidgetToArea("library", CustomizableUI.AREA_NAVBAR, 0);
});

