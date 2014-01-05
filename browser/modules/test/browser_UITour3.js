/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;

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

    gContentWindow = Components.utils.waiveXrays(gTestTab.linkedBrowser.contentDocument.defaultView);
    gContentAPI = gContentWindow.Mozilla.UITour;

    waitForFocus(callback, gContentWindow);
  }, true);
}

function test() {
  Services.prefs.setBoolPref("browser.uitour.enabled", true);
  let testUri = Services.io.newURI("http://example.com", null, null);
  Services.perms.add(testUri, "uitour", Services.perms.ALLOW_ACTION);

  waitForExplicitFinish();

  registerCleanupFunction(function() {
    delete window.UITour;
    delete window.gContentWindow;
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
  function test_info_icon(done) {
    let popup = document.getElementById("UITourTooltip");
    let title = document.getElementById("UITourTooltipTitle");
    let desc = document.getElementById("UITourTooltipDescription");
    let icon = document.getElementById("UITourTooltipIcon");
    let buttons = document.getElementById("UITourTooltipButtons");

    popup.addEventListener("popupshown", function onPopupShown() {
      popup.removeEventListener("popupshown", onPopupShown);

      is(title.textContent, "a title", "Popup should have correct title");
      is(desc.textContent, "some text", "Popup should have correct description text");

      let imageURL = getRootDirectory(gTestPath) + "image.png";
      imageURL = imageURL.replace("chrome://mochitests/content/", "https://example.com/");
      is(icon.src, imageURL,  "Popup should have correct icon shown");

      is(buttons.hasChildNodes(), false, "Popup should have no buttons");

      done();
    });

    gContentAPI.showInfo("urlbar", "a title", "some text", "image.png");
  },
  function test_info_buttons_1(done) {
    let popup = document.getElementById("UITourTooltip");
    let title = document.getElementById("UITourTooltipTitle");
    let desc = document.getElementById("UITourTooltipDescription");
    let icon = document.getElementById("UITourTooltipIcon");

    popup.addEventListener("popupshown", function onPopupShown() {
      popup.removeEventListener("popupshown", onPopupShown);

      is(title.textContent, "another title", "Popup should have correct title");
      is(desc.textContent, "moar text", "Popup should have correct description text");

      let imageURL = getRootDirectory(gTestPath) + "image.png";
      imageURL = imageURL.replace("chrome://mochitests/content/", "https://example.com/");
      is(icon.src, imageURL,  "Popup should have correct icon shown");

      let buttons = document.getElementById("UITourTooltipButtons");
      is(buttons.childElementCount, 2, "Popup should have two buttons");

      is(buttons.childNodes[0].getAttribute("label"), "Button 1", "First button should have correct label");
      is(buttons.childNodes[0].getAttribute("image"), "", "First button should have no image");

      is(buttons.childNodes[1].getAttribute("label"), "Button 2", "Second button should have correct label");
      is(buttons.childNodes[1].getAttribute("image"), imageURL, "Second button should have correct image");

      popup.addEventListener("popuphidden", function onPopupHidden() {
        popup.removeEventListener("popuphidden", onPopupHidden);
        ok(true, "Popup should close automatically");

        executeSoon(function() {
          is(gContentWindow.callbackResult, "button1", "Correct callback should have been called");

          done();
        });
      });

      EventUtils.synthesizeMouseAtCenter(buttons.childNodes[0], {}, window);
    });

    let buttons = gContentWindow.makeButtons();
    gContentAPI.showInfo("urlbar", "another title", "moar text", "./image.png", buttons);
  },
  function test_info_buttons_2(done) {
    let popup = document.getElementById("UITourTooltip");
    let title = document.getElementById("UITourTooltipTitle");
    let desc = document.getElementById("UITourTooltipDescription");
    let icon = document.getElementById("UITourTooltipIcon");

    popup.addEventListener("popupshown", function onPopupShown() {
      popup.removeEventListener("popupshown", onPopupShown);

      is(title.textContent, "another title", "Popup should have correct title");
      is(desc.textContent, "moar text", "Popup should have correct description text");

      let imageURL = getRootDirectory(gTestPath) + "image.png";
      imageURL = imageURL.replace("chrome://mochitests/content/", "https://example.com/");
      is(icon.src, imageURL,  "Popup should have correct icon shown");

      let buttons = document.getElementById("UITourTooltipButtons");
      is(buttons.childElementCount, 2, "Popup should have two buttons");

      is(buttons.childNodes[0].getAttribute("label"), "Button 1", "First button should have correct label");
      is(buttons.childNodes[0].getAttribute("image"), "", "First button should have no image");

      is(buttons.childNodes[1].getAttribute("label"), "Button 2", "Second button should have correct label");
      is(buttons.childNodes[1].getAttribute("image"), imageURL, "Second button should have correct image");

      popup.addEventListener("popuphidden", function onPopupHidden() {
        popup.removeEventListener("popuphidden", onPopupHidden);
        ok(true, "Popup should close automatically");

        executeSoon(function() {
          is(gContentWindow.callbackResult, "button2", "Correct callback should have been called");

          done();
        });
      });

      EventUtils.synthesizeMouseAtCenter(buttons.childNodes[1], {}, window);
    });

    let buttons = gContentWindow.makeButtons();
    gContentAPI.showInfo("urlbar", "another title", "moar text", "./image.png", buttons);
  },
];
