/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;

Components.utils.import("resource:///modules/UITour.jsm");

function test() {
  UITourTest();
}

let tests = [
  function test_info_customize_auto_open_close(done) {
    let popup = document.getElementById("UITourTooltip");
    gContentAPI.showInfo("customize", "Customization", "Customize me please!");
    UITour.getTarget(window, "customize").then((customizeTarget) => {
      waitForPopupAtAnchor(popup, customizeTarget.node, function checkPanelIsOpen() {
        isnot(PanelUI.panel.state, "closed", "Panel should have opened before the popup anchored");
        ok(PanelUI.panel.hasAttribute("noautohide"), "@noautohide on the menu panel should have been set");

        // Move the info outside which should close the app menu.
        gContentAPI.showInfo("appMenu", "Open Me", "You know you want to");
        UITour.getTarget(window, "appMenu").then((target) => {
          waitForPopupAtAnchor(popup, target.node, function checkPanelIsClosed() {
            isnot(PanelUI.panel.state, "open",
                  "Panel should have closed after the info moved elsewhere.");
            ok(!PanelUI.panel.hasAttribute("noautohide"), "@noautohide on the menu panel should have been cleaned up on close");
            done();
          }, "Info should move to the appMenu button");
        });
      }, "Info panel should be anchored to the customize button");
    });
  },
  function test_info_customize_manual_open_close(done) {
    let popup = document.getElementById("UITourTooltip");
    // Manually open the app menu then show an info panel there. The menu should remain open.
    gContentAPI.showMenu("appMenu");
    isnot(PanelUI.panel.state, "closed", "Panel should have opened");
    ok(PanelUI.panel.hasAttribute("noautohide"), "@noautohide on the menu panel should have been set");
    gContentAPI.showInfo("customize", "Customization", "Customize me please!");

    UITour.getTarget(window, "customize").then((customizeTarget) => {
      waitForPopupAtAnchor(popup, customizeTarget.node, function checkMenuIsStillOpen() {
        isnot(PanelUI.panel.state, "closed", "Panel should still be open");
        ok(PanelUI.panel.hasAttribute("noautohide"), "@noautohide on the menu panel should still be set");

        // Move the info outside which shouldn't close the app menu since it was manually opened.
        gContentAPI.showInfo("appMenu", "Open Me", "You know you want to");
        UITour.getTarget(window, "appMenu").then((target) => {
          waitForPopupAtAnchor(popup, target.node, function checkMenuIsStillOpen() {
            isnot(PanelUI.panel.state, "closed",
                  "Menu should remain open since UITour didn't open it in the first place");
            gContentAPI.hideMenu("appMenu");
            ok(!PanelUI.panel.hasAttribute("noautohide"), "@noautohide on the menu panel should have been cleaned up on close");
            done();
          }, "Info should move to the appMenu button");
        });
      }, "Info should be shown after showInfo() for fixed menu panel items");
    });
  },
  function test_pinnedTab(done) {
    is(UITour.pinnedTabs.get(window), null, "Should not already have a pinned tab");

    gContentAPI.addPinnedTab();
    let tabInfo = UITour.pinnedTabs.get(window);
    isnot(tabInfo, null, "Should have recorded data about a pinned tab after addPinnedTab()");
    isnot(tabInfo.tab, null, "Should have added a pinned tab after addPinnedTab()");
    is(tabInfo.tab.pinned, true, "Tab should be marked as pinned");

    let tab = tabInfo.tab;

    gContentAPI.removePinnedTab();
    isnot(gBrowser.tabs[0], tab, "First tab should not be the pinned tab");
    let tabInfo = UITour.pinnedTabs.get(window);
    is(tabInfo, null, "Should not have any data about the removed pinned tab after removePinnedTab()");

    gContentAPI.addPinnedTab();
    gContentAPI.addPinnedTab();
    gContentAPI.addPinnedTab();
    is(gBrowser.tabs[1].pinned, false, "After multiple calls of addPinnedTab, should still only have one pinned tab");

    done();
  },
  function test_menu(done) {
    let bookmarksMenuButton = document.getElementById("bookmarks-menu-button");
    ise(bookmarksMenuButton.open, false, "Menu should initially be closed");

    gContentAPI.showMenu("bookmarks");
    ise(bookmarksMenuButton.open, true, "Menu should be shown after showMenu()");

    gContentAPI.hideMenu("bookmarks");
    ise(bookmarksMenuButton.open, false, "Menu should be closed after hideMenu()");

    done();
  },
];
