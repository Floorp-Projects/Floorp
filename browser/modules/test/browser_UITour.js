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
  function test_untrusted_host(done) {
    loadTestPage(function() {
      let bookmarksMenu = document.getElementById("bookmarks-menu-button");
      ise(bookmarksMenu.open, false, "Bookmark menu should initially be closed");

      gContentAPI.showMenu("bookmarks");
      ise(bookmarksMenu.open, false, "Bookmark menu should not open on a untrusted host");

      done();
    }, "http://mochi.test:8888/");
  },
  function test_unsecure_host(done) {
    loadTestPage(function() {
      let bookmarksMenu = document.getElementById("bookmarks-menu-button");
      ise(bookmarksMenu.open, false, "Bookmark menu should initially be closed");

      gContentAPI.showMenu("bookmarks");
      ise(bookmarksMenu.open, false, "Bookmark menu should not open on a unsecure host");

      done();
    }, "http://example.com/");
  },
  function test_unsecure_host_override(done) {
    Services.prefs.setBoolPref("browser.uitour.requireSecure", false);
    loadTestPage(function() {
      let highlight = document.getElementById("UITourHighlight");
      is_element_hidden(highlight, "Highlight should initially be hidden");

      gContentAPI.showHighlight("urlbar");
      waitForElementToBeVisible(highlight, done, "Highlight should be shown on a unsecure host when override pref is set");

      Services.prefs.setBoolPref("browser.uitour.requireSecure", true);
    }, "http://example.com/");
  },
  function test_disabled(done) {
    Services.prefs.setBoolPref("browser.uitour.enabled", false);

    let bookmarksMenu = document.getElementById("bookmarks-menu-button");
    ise(bookmarksMenu.open, false, "Bookmark menu should initially be closed");

    gContentAPI.showMenu("bookmarks");
    ise(bookmarksMenu.open, false, "Bookmark menu should not open when feature is disabled");

    Services.prefs.setBoolPref("browser.uitour.enabled", true);
    done();
  },
  function test_highlight(done) {
    function test_highlight_2() {
      let highlight = document.getElementById("UITourHighlight");
      gContentAPI.hideHighlight();
      is_element_hidden(highlight, "Highlight should be hidden after hideHighlight()");

      gContentAPI.showHighlight("urlbar");
      waitForElementToBeVisible(highlight, test_highlight_3, "Highlight should be shown after showHighlight()");
    }
    function test_highlight_3() {
      let highlight = document.getElementById("UITourHighlight");
      gContentAPI.showHighlight("backForward");
      waitForElementToBeVisible(highlight, done, "Highlight should be shown after showHighlight()");
    }

    let highlight = document.getElementById("UITourHighlight");
    is_element_hidden(highlight, "Highlight should initially be hidden");

    gContentAPI.showHighlight("urlbar");
    waitForElementToBeVisible(highlight, test_highlight_2, "Highlight should be shown after showHighlight()");
  },
  function test_highlight_customize_auto_open_close(done) {
    let highlight = document.getElementById("UITourHighlight");
    gContentAPI.showHighlight("customize");
    waitForElementToBeVisible(highlight, function checkPanelIsOpen() {
      isnot(PanelUI.panel.state, "closed", "Panel should have opened");

      // Move the highlight outside which should close the app menu.
      gContentAPI.showHighlight("appMenu");
      waitForElementToBeVisible(highlight, function checkPanelIsClosed() {
        isnot(PanelUI.panel.state, "open",
              "Panel should have closed after the highlight moved elsewhere.");
        done();
      }, "Highlight should move to the appMenu button");
    }, "Highlight should be shown after showHighlight() for fixed panel items");
  },
  function test_highlight_customize_manual_open_close(done) {
    let highlight = document.getElementById("UITourHighlight");
    // Manually open the app menu then show a highlight there. The menu should remain open.
    gContentAPI.showMenu("appMenu");
    isnot(PanelUI.panel.state, "closed", "Panel should have opened");
    gContentAPI.showHighlight("customize");

    waitForElementToBeVisible(highlight, function checkPanelIsStillOpen() {
      isnot(PanelUI.panel.state, "closed", "Panel should still be open");

      // Move the highlight outside which shouldn't close the app menu since it was manually opened.
      gContentAPI.showHighlight("appMenu");
      waitForElementToBeVisible(highlight, function () {
        isnot(PanelUI.panel.state, "closed",
              "Panel should remain open since UITour didn't open it in the first place");
        gContentAPI.hideMenu("appMenu");
        done();
      }, "Highlight should move to the appMenu button");
    }, "Highlight should be shown after showHighlight() for fixed panel items");
  },
  function test_highlight_effect(done) {
    function waitForHighlightWithEffect(highlightEl, effect, next, error) {
      return waitForCondition(() => highlightEl.getAttribute("active") == effect,
                              next,
                              error);
    }
    function checkDefaultEffect() {
      is(highlight.getAttribute("active"), "none", "The default should be no effect");

      gContentAPI.showHighlight("urlbar", "none");
      waitForHighlightWithEffect(highlight, "none", checkZoomEffect, "There should be no effect");
    }
    function checkZoomEffect() {
      gContentAPI.showHighlight("urlbar", "zoom");
      waitForHighlightWithEffect(highlight, "zoom", () => {
        let style = window.getComputedStyle(highlight);
        is(style.animationName, "uitour-zoom", "The animation-name should be uitour-zoom");
        checkRandomEffect();
      }, "There should be a zoom effect");
    }
    function checkRandomEffect() {
      function waitForActiveHighlight(highlightEl, next, error) {
        return waitForCondition(() => highlightEl.hasAttribute("active"),
                                next,
                                error);
      }

      gContentAPI.hideHighlight();
      gContentAPI.showHighlight("urlbar", "random");
      waitForActiveHighlight(highlight, () => {
        ok(highlight.hasAttribute("active"), "The highlight should be active");
        isnot(highlight.getAttribute("active"), "none", "A random effect other than none should have been chosen");
        isnot(highlight.getAttribute("active"), "random", "The random effect shouldn't be 'random'");
        isnot(UITour.highlightEffects.indexOf(highlight.getAttribute("active")), -1, "Check that a supported effect was randomly chosen");
        done();
      }, "There should be an active highlight with a random effect");
    }

    let highlight = document.getElementById("UITourHighlight");
    is_element_hidden(highlight, "Highlight should initially be hidden");

    gContentAPI.showHighlight("urlbar");
    waitForElementToBeVisible(highlight, checkDefaultEffect, "Highlight should be shown after showHighlight()");
  },
  function test_highlight_effect_unsupported(done) {
    function checkUnsupportedEffect() {
      is(highlight.getAttribute("active"), "none", "No effect should be used when an unsupported effect is requested");
      done();
    }

    let highlight = document.getElementById("UITourHighlight");
    is_element_hidden(highlight, "Highlight should initially be hidden");

    gContentAPI.showHighlight("urlbar", "__UNSUPPORTED__");
    waitForElementToBeVisible(highlight, checkUnsupportedEffect, "Highlight should be shown after showHighlight()");
  },
  function test_info_1(done) {
    let popup = document.getElementById("UITourTooltip");
    let title = document.getElementById("UITourTooltipTitle");
    let desc = document.getElementById("UITourTooltipDescription");
    popup.addEventListener("popupshown", function onPopupShown() {
      popup.removeEventListener("popupshown", onPopupShown);
      is(popup.popupBoxObject.anchorNode, document.getElementById("urlbar"), "Popup should be anchored to the urlbar");
      is(title.textContent, "test title", "Popup should have correct title");
      is(desc.textContent, "test text", "Popup should have correct description text");

      popup.addEventListener("popuphidden", function onPopupHidden() {
        popup.removeEventListener("popuphidden", onPopupHidden);

        popup.addEventListener("popupshown", function onPopupShown() {
          popup.removeEventListener("popupshown", onPopupShown);
          done();
        });

        gContentAPI.showInfo("urlbar", "test title", "test text");

      });
      gContentAPI.hideInfo();
    });

    gContentAPI.showInfo("urlbar", "test title", "test text");
  },
  function test_info_2(done) {
    let popup = document.getElementById("UITourTooltip");
    let title = document.getElementById("UITourTooltipTitle");
    let desc = document.getElementById("UITourTooltipDescription");
    popup.addEventListener("popupshown", function onPopupShown() {
      popup.removeEventListener("popupshown", onPopupShown);
      is(popup.popupBoxObject.anchorNode, document.getElementById("urlbar"), "Popup should be anchored to the urlbar");
      is(title.textContent, "urlbar title", "Popup should have correct title");
      is(desc.textContent, "urlbar text", "Popup should have correct description text");

      gContentAPI.showInfo("search", "search title", "search text");
      executeSoon(function() {
        is(popup.popupBoxObject.anchorNode, document.getElementById("searchbar"), "Popup should be anchored to the searchbar");
        is(title.textContent, "search title", "Popup should have correct title");
        is(desc.textContent, "search text", "Popup should have correct description text");

        done();
      });
    });

    gContentAPI.showInfo("urlbar", "urlbar title", "urlbar text");
  },
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
