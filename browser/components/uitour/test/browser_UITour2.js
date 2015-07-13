/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;

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
    let shownPromise = promisePanelShown(window);
    gContentAPI.showMenu("appMenu");
    shownPromise.then(() => {
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
              waitForElementToBeHidden(window.PanelUI.panel, () => {
                ok(!PanelUI.panel.hasAttribute("noautohide"), "@noautohide on the menu panel should have been cleaned up on close");
                done();
              });
              gContentAPI.hideMenu("appMenu");
            }, "Info should move to the appMenu button");
          });
        }, "Info should be shown after showInfo() for fixed menu panel items");
      });
    }).then(null, Components.utils.reportError);
  },
  taskify(function* test_bookmarks_menu() {
    let bookmarksMenuButton = document.getElementById("bookmarks-menu-button");

    is(bookmarksMenuButton.open, false, "Menu should initially be closed");
    gContentAPI.showMenu("bookmarks");

    yield waitForConditionPromise(() => {
      return bookmarksMenuButton.open;
    }, "Menu should be visible after showMenu()");

    gContentAPI.hideMenu("bookmarks");
    yield waitForConditionPromise(() => {
        return !bookmarksMenuButton.open;
    }, "Menu should be hidden after hideMenu()");
  }),
];
