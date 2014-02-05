/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that annotations disappear when their target is hidden.
 */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;
let highlight = document.getElementById("UITourHighlight");
let tooltip = document.getElementById("UITourTooltip");

Components.utils.import("resource:///modules/UITour.jsm");

function test() {
  registerCleanupFunction(() => {
    // Close the find bar in case it's open in the remaining tab
    gBrowser.getFindBar(gBrowser.selectedTab).close();
  });
  UITourTest();
}

let tests = [
  function test_highlight_move_outside_panel(done) {
    gContentAPI.showInfo("urlbar", "test title", "test text");
    gContentAPI.showHighlight("customize");
    waitForElementToBeVisible(highlight, function checkPanelIsOpen() {
      isnot(PanelUI.panel.state, "closed", "Panel should have opened");

      // Move the highlight outside which should close the app menu.
      gContentAPI.showHighlight("appMenu");
      waitForPopupAtAnchor(highlight.parentElement, document.getElementById("PanelUI-menu-button"), () => {
        isnot(PanelUI.panel.state, "open",
              "Panel should have closed after the highlight moved elsewhere.");
        is(tooltip.state, "open", "The info panel should have remained open");
        done();
      }, "Highlight should move to the appMenu button and still be visible");
    }, "Highlight should be shown after showHighlight() for fixed panel items");
  },

  function test_highlight_panel_hideMenu(done) {
    gContentAPI.showHighlight("customize");
    gContentAPI.showInfo("search", "test title", "test text");
    waitForElementToBeVisible(highlight, function checkPanelIsOpen() {
      isnot(PanelUI.panel.state, "closed", "Panel should have opened");

      // Close the app menu and make sure the highlight also disappeared.
      gContentAPI.hideMenu("appMenu");
      waitForElementToBeHidden(highlight, function checkPanelIsClosed() {
        isnot(PanelUI.panel.state, "open",
              "Panel still should have closed");
        is(tooltip.state, "open", "The info panel should have remained open");
        done();
      }, "Highlight should have disappeared when panel closed");
    }, "Highlight should be shown after showHighlight() for fixed panel items");
  },

  function test_highlight_panel_click_find(done) {
    gContentAPI.showHighlight("help");
    gContentAPI.showInfo("searchProvider", "test title", "test text");
    waitForElementToBeVisible(highlight, function checkPanelIsOpen() {
      isnot(PanelUI.panel.state, "closed", "Panel should have opened");

      // Click the find button which should close the panel.
      let findButton = document.getElementById("find-button");
      EventUtils.synthesizeMouseAtCenter(findButton, {});
      waitForElementToBeHidden(highlight, function checkPanelIsClosed() {
        isnot(PanelUI.panel.state, "open",
              "Panel should have closed when the find bar opened");
        is(tooltip.state, "open", "The info panel should have remained open");
        done();
      }, "Highlight should have disappeared when panel closed");
    }, "Highlight should be shown after showHighlight() for fixed panel items");
  },

  function test_highlight_info_panel_click_find(done) {
    gContentAPI.showHighlight("help");
    gContentAPI.showInfo("customize", "customize me!", "awesome!");
    waitForElementToBeVisible(highlight, function checkPanelIsOpen() {
      isnot(PanelUI.panel.state, "closed", "Panel should have opened");

      // Click the find button which should close the panel.
      let findButton = document.getElementById("find-button");
      EventUtils.synthesizeMouseAtCenter(findButton, {});
      waitForElementToBeHidden(highlight, function checkPanelIsClosed() {
        isnot(PanelUI.panel.state, "open",
              "Panel should have closed when the find bar opened");
        waitForElementToBeHidden(tooltip, function checkTooltipIsClosed() {
          isnot(tooltip.state, "open", "The info panel should have closed too");
          done();
        }, "Tooltip should hide with the menu");
      }, "Highlight should have disappeared when panel closed");
    }, "Highlight should be shown after showHighlight() for fixed panel items");
  },

  function test_info_move_outside_panel(done) {
    gContentAPI.showInfo("addons", "test title", "test text");
    gContentAPI.showHighlight("urlbar");
    let addonsButton = document.getElementById("add-ons-button");
    waitForPopupAtAnchor(tooltip, addonsButton, function checkPanelIsOpen() {
      isnot(PanelUI.panel.state, "closed", "Panel should have opened");

      // Move the info panel outside which should close the app menu.
      gContentAPI.showInfo("appMenu", "Cool menu button", "It's three lines");
      waitForPopupAtAnchor(tooltip, document.getElementById("PanelUI-menu-button"), () => {
        isnot(PanelUI.panel.state, "open",
              "Menu should have closed after the highlight moved elsewhere.");
        is(highlight.parentElement.state, "open", "The highlight should have remained visible");
        done();
      }, "Tooltip should move to the appMenu button and still be visible");
    }, "Tooltip should be shown after showInfo() for a panel item");
  },

];
