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
  function test_untrusted_host(done) {
    loadUITourTestPage(function() {
      let bookmarksMenu = document.getElementById("bookmarks-menu-button");
      ise(bookmarksMenu.open, false, "Bookmark menu should initially be closed");

      gContentAPI.showMenu("bookmarks");
      ise(bookmarksMenu.open, false, "Bookmark menu should not open on a untrusted host");

      done();
    }, "http://mochi.test:8888/");
  },
  function test_unsecure_host(done) {
    loadUITourTestPage(function() {
      let bookmarksMenu = document.getElementById("bookmarks-menu-button");
      ise(bookmarksMenu.open, false, "Bookmark menu should initially be closed");

      gContentAPI.showMenu("bookmarks");
      ise(bookmarksMenu.open, false, "Bookmark menu should not open on a unsecure host");

      done();
    }, "http://example.com/");
  },
  function test_unsecure_host_override(done) {
    Services.prefs.setBoolPref("browser.uitour.requireSecure", false);
    loadUITourTestPage(function() {
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
  function test_highlight_circle(done) {
    function check_highlight_size() {
      let panel = highlight.parentElement;
      let anchor = panel.anchorNode;
      let anchorRect = anchor.getBoundingClientRect();
      info("addons target: width: " + anchorRect.width + " height: " + anchorRect.height);
      let maxDimension = Math.round(Math.max(anchorRect.width, anchorRect.height));
      let highlightRect = highlight.getBoundingClientRect();
      info("highlight: width: " + highlightRect.width + " height: " + highlightRect.height);
      is(Math.round(highlightRect.width), maxDimension, "The width of the highlight should be equal to the largest dimension of the target");
      is(Math.round(highlightRect.height), maxDimension, "The height of the highlight should be equal to the largest dimension of the target");
      is(Math.round(highlightRect.height), Math.round(highlightRect.width), "The height and width of the highlight should be the same to create a circle");
      is(highlight.style.borderRadius, "100%", "The border-radius should be 100% to create a circle");
      done();
    }
    let highlight = document.getElementById("UITourHighlight");
    is_element_hidden(highlight, "Highlight should initially be hidden");

    gContentAPI.showHighlight("addons");
    waitForElementToBeVisible(highlight, check_highlight_size, "Highlight should be shown after showHighlight()");
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
    let shownPromise = promisePanelShown(window);
    gContentAPI.showMenu("appMenu");
    shownPromise.then(() => {
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
    }).then(null, Components.utils.reportError);
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
        checkSameEffectOnDifferentTarget();
      }, "There should be a zoom effect");
    }
    function checkSameEffectOnDifferentTarget() {
      gContentAPI.showHighlight("appMenu", "wobble");
      waitForHighlightWithEffect(highlight, "wobble", () => {
        highlight.addEventListener("animationstart", function onAnimationStart(aEvent) {
          highlight.removeEventListener("animationstart", onAnimationStart);
          ok(true, "Animation occurred again even though the effect was the same");
          checkRandomEffect();
        });
        gContentAPI.showHighlight("backForward", "wobble");
      }, "There should be a wobble effect");
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
    let icon = document.getElementById("UITourTooltipIcon");
    let buttons = document.getElementById("UITourTooltipButtons");

    popup.addEventListener("popupshown", function onPopupShown() {
      popup.removeEventListener("popupshown", onPopupShown);
      is(popup.popupBoxObject.anchorNode, document.getElementById("urlbar"), "Popup should be anchored to the urlbar");
      is(title.textContent, "test title", "Popup should have correct title");
      is(desc.textContent, "test text", "Popup should have correct description text");
      is(icon.src, "", "Popup should have no icon");
      is(buttons.hasChildNodes(), false, "Popup should have no buttons");

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
    let icon = document.getElementById("UITourTooltipIcon");
    let buttons = document.getElementById("UITourTooltipButtons");

    popup.addEventListener("popupshown", function onPopupShown() {
      popup.removeEventListener("popupshown", onPopupShown);
      is(popup.popupBoxObject.anchorNode, document.getElementById("urlbar"), "Popup should be anchored to the urlbar");
      is(title.textContent, "urlbar title", "Popup should have correct title");
      is(desc.textContent, "urlbar text", "Popup should have correct description text");
      is(icon.src, "", "Popup should have no icon");
      is(buttons.hasChildNodes(), false, "Popup should have no buttons");

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

  // Make sure this test is last in the file so the appMenu gets left open and done will confirm it got tore down.
  function cleanupMenus(done) {
    gContentAPI.showMenu("appMenu");
    done();
  },
];
