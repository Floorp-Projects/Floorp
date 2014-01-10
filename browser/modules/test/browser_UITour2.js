/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTestTab;
let gContentAPI;

Components.utils.import("resource:///modules/UITour.jsm");

function loadTestPage(callback, host = "https://example.com/") {
   if (gTestTab)
    gBrowser.removeTab(gTestTab);

  let url = getRootDirectory(gTestPath) + "uitour.html";
  url = url.replace("chrome://mochitests/content/", host);

  gTestTab = gBrowser.addTab(url);
  gBrowser.selectedTab = gTestTab;

  gTestTab.linkedBrowser.addEventListener("load", function onLoad() {
    gTestTab.linkedBrowser.removeEventListener("load", onLoad);

    let contentWindow = Components.utils.waiveXrays(gTestTab.linkedBrowser.contentDocument.defaultView);
    gContentAPI = contentWindow.Mozilla.UITour;

    waitForFocus(callback, contentWindow);
  }, true);
}

function test() {
  Services.prefs.setBoolPref("browser.uitour.enabled", true);
  let testUri = Services.io.newURI("http://example.com", null, null);
  Services.perms.add(testUri, "uitour", Services.perms.ALLOW_ACTION);

  waitForExplicitFinish();

  registerCleanupFunction(function() {
    delete window.UITour;
    delete window.gContentAPI;
    if (gTestTab)
      gBrowser.removeTab(gTestTab);
    delete window.gTestTab;
    Services.prefs.clearUserPref("browser.uitour.enabled", true);
    Services.perms.remove("example.com", "uitour");
  });

  function done() {
    if (gTestTab)
      gBrowser.removeTab(gTestTab);
    gTestTab = null;

    let highlight = document.getElementById("UITourHighlightContainer");
    is_element_hidden(highlight, "Highlight should be closed/hidden after UITour tab is closed");

    let tooltip = document.getElementById("UITourTooltip");
    is_element_hidden(tooltip, "Tooltip should be closed/hidden after UITour tab is closed");

    ok(!PanelUI.panel.hasAttribute("noautohide"), "@noautohide on the menu panel should have been cleaned up");

    is(UITour.pinnedTabs.get(window), null, "Any pinned tab should be closed after UITour tab is closed");

    executeSoon(nextTest);
  }

  function nextTest() {
    if (tests.length == 0) {
      finish();
      return;
    }
    let test = tests.shift();
    info("Starting " + test.name);
    loadTestPage(function() {
      test(done);
    });
  }
  nextTest();
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
