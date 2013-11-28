/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTestTab;
let gContentAPI;

Components.utils.import("resource:///modules/UITour.jsm");

function is_hidden(element) {
  var style = element.ownerDocument.defaultView.getComputedStyle(element, "");
  if (style.display == "none")
    return true;
  if (style.visibility != "visible")
    return true;

  // Hiding a parent element will hide all its children
  if (element.parentNode != element.ownerDocument)
    return is_hidden(element.parentNode);

  return false;
}

function is_element_visible(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(!is_hidden(element), msg);
}

function is_element_hidden(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(is_hidden(element), msg);
}

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

    let highlight = document.getElementById("UITourHighlight");
    is_element_hidden(highlight, "Highlight should be hidden after UITour tab is closed");

    let popup = document.getElementById("UITourTooltip");
    isnot(["hidding","closed"].indexOf(popup.state), -1, "Popup should be closed/hidding after UITour tab is closed");

    is(UITour.pinnedTabs.get(window), null, "Any pinned tab should be closed after UITour tab is closed");

    executeSoon(nextTest);
  }

  function nextTest() {
    if (tests.length == 0) {
      finish();
      return;
    }
    let test = tests.shift();

    loadTestPage(function() {
      test(done);
    });
  }
  nextTest();
}

let tests = [
  function test_untrusted_host(done) {
    loadTestPage(function() {
      let highlight = document.getElementById("UITourHighlight");
      is_element_hidden(highlight, "Highlight should initially be hidden");

      gContentAPI.showHighlight("urlbar");
      is_element_hidden(highlight, "Highlight should not be shown on a untrusted host");

      done();
    }, "http://mochi.test:8888/");
  },
  function test_unsecure_host(done) {
    loadTestPage(function() {
      let highlight = document.getElementById("UITourHighlight");
      is_element_hidden(highlight, "Highlight should initially be hidden");

      gContentAPI.showHighlight("urlbar");
      is_element_hidden(highlight, "Highlight should not be shown on a unsecure host");

      done();
    }, "http://example.com/");
  },
  function test_unsecure_host_override(done) {
    Services.prefs.setBoolPref("browser.uitour.requireSecure", false);
    loadTestPage(function() {
      let highlight = document.getElementById("UITourHighlight");
      is_element_hidden(highlight, "Highlight should initially be hidden");

      gContentAPI.showHighlight("urlbar");
      is_element_visible(highlight, "Highlight should be shown on a unsecure host when override pref is set");

      Services.prefs.setBoolPref("browser.uitour.requireSecure", true);
      done();
    }, "http://example.com/");
  },
  function test_disabled(done) {
    Services.prefs.setBoolPref("browser.uitour.enabled", false);

    let highlight = document.getElementById("UITourHighlight");
    is_element_hidden(highlight, "Highlight should initially be hidden");

    gContentAPI.showHighlight("urlbar");
    is_element_hidden(highlight, "Highlight should not be shown when feature is disabled");

    Services.prefs.setBoolPref("browser.uitour.enabled", true);
    done();
  },
  function test_highlight(done) {
    let highlight = document.getElementById("UITourHighlight");
    is_element_hidden(highlight, "Highlight should initially be hidden");

    gContentAPI.showHighlight("urlbar");
    is_element_visible(highlight, "Highlight should be shown after showHighlight()");

    gContentAPI.hideHighlight();
    is_element_hidden(highlight, "Highlight should be hidden after hideHighlight()");

    gContentAPI.showHighlight("urlbar");
    is_element_visible(highlight, "Highlight should be shown after showHighlight()");
    gContentAPI.showHighlight("backforward");
    is_element_visible(highlight, "Highlight should be shown after showHighlight()");

    done();
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
