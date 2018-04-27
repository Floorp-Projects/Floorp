/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

ChromeUtils.import("resource://testing-common/TelemetryArchiveTesting.jsm", this);
ChromeUtils.import("resource://gre/modules/ProfileAge.jsm", this);
ChromeUtils.import("resource://gre/modules/UpdateUtils.jsm", this);


function test() {
  UITourTest();
}

var tests = [
  function test_untrusted_host(done) {
    loadUITourTestPage(function() {
      CustomizableUI.addWidgetToArea("bookmarks-menu-button", CustomizableUI.AREA_NAVBAR, 0);
      registerCleanupFunction(() => CustomizableUI.removeWidgetFromArea("bookmarks-menu-button"));
      let bookmarksMenu = document.getElementById("bookmarks-menu-button");
      is(bookmarksMenu.open, false, "Bookmark menu should initially be closed");

      gContentAPI.showMenu("bookmarks");
      is(bookmarksMenu.open, false, "Bookmark menu should not open on a untrusted host");

      done();
    }, "http://mochi.test:8888/");
  },
  function test_testing_host(done) {
    // Add two testing origins intentionally surrounded by whitespace to be ignored.
    Services.prefs.setCharPref("browser.uitour.testingOrigins",
                               "https://test1.example.org, https://test2.example.org:443 ");

    registerCleanupFunction(() => {
      Services.prefs.clearUserPref("browser.uitour.testingOrigins");
    });
    function callback(result) {
      ok(result, "Callback should be called on a testing origin");
      done();
    }

    loadUITourTestPage(function() {
      gContentAPI.getConfiguration("appinfo", callback);
    }, "https://test2.example.org/");
  },
  function test_unsecure_host(done) {
    loadUITourTestPage(function() {
      let bookmarksMenu = document.getElementById("bookmarks-menu-button");
      is(bookmarksMenu.open, false, "Bookmark menu should initially be closed");

      gContentAPI.showMenu("bookmarks");
      is(bookmarksMenu.open, false, "Bookmark menu should not open on a unsecure host");

      done();
    }, "http://example.org/");
  },
  function test_unsecure_host_override(done) {
    Services.prefs.setBoolPref("browser.uitour.requireSecure", false);
    loadUITourTestPage(function() {
      let highlight = document.getElementById("UITourHighlight");
      is_element_hidden(highlight, "Highlight should initially be hidden");

      gContentAPI.showHighlight("urlbar");
      waitForElementToBeVisible(highlight, done, "Highlight should be shown on a unsecure host when override pref is set");

      Services.prefs.setBoolPref("browser.uitour.requireSecure", true);
    }, "http://example.org/");
  },
  function test_disabled(done) {
    Services.prefs.setBoolPref("browser.uitour.enabled", false);

    let bookmarksMenu = document.getElementById("bookmarks-menu-button");
    is(bookmarksMenu.open, false, "Bookmark menu should initially be closed");

    gContentAPI.showMenu("bookmarks");
    is(bookmarksMenu.open, false, "Bookmark menu should not open when feature is disabled");

    Services.prefs.setBoolPref("browser.uitour.enabled", true);
    done();
  },
  function test_highlight(done) {
    function test_highlight_2() {
      let highlight = document.getElementById("UITourHighlight");
      gContentAPI.hideHighlight();

      waitForElementToBeHidden(highlight, test_highlight_3, "Highlight should be hidden after hideHighlight()");
    }
    function test_highlight_3() {
      is_element_hidden(highlight, "Highlight should be hidden after hideHighlight()");

      gContentAPI.showHighlight("urlbar");
      waitForElementToBeVisible(highlight, test_highlight_4, "Highlight should be shown after showHighlight()");
    }
    function test_highlight_4() {
      let highlight = document.getElementById("UITourHighlight");
      gContentAPI.showHighlight("backForward");
      waitForElementToBeVisible(highlight, done, "Highlight should be shown after showHighlight()");
    }

    let highlight = document.getElementById("UITourHighlight");
    is_element_hidden(highlight, "Highlight should initially be hidden");

    gContentAPI.showHighlight("urlbar");
    waitForElementToBeVisible(highlight, test_highlight_2, "Highlight should be shown after showHighlight()");
  },
  function test_highlight_toolbar_button(done) {
    function check_highlight_size() {
      let panel = highlight.parentElement;
      let anchor = panel.anchorNode;
      let anchorRect = anchor.getBoundingClientRect();
      info("addons target: width: " + anchorRect.width + " height: " + anchorRect.height);
      let dimension = anchorRect.width;
      let highlightRect = highlight.getBoundingClientRect();
      info("highlight: width: " + highlightRect.width + " height: " + highlightRect.height);
      is(Math.round(highlightRect.width), dimension, "The width of the highlight should be equal to the width of the target");
      is(Math.round(highlightRect.height), dimension, "The height of the highlight should be equal to the width of the target");
      is(highlight.classList.contains("rounded-highlight"), true, "Highlight should be rounded-rectangle styled");
      done();
    }
    let highlight = document.getElementById("UITourHighlight");
    is_element_hidden(highlight, "Highlight should initially be hidden");

    gContentAPI.showHighlight("home");
    waitForElementToBeVisible(highlight, check_highlight_size, "Highlight should be shown after showHighlight()");
  },
  function test_highlight_customize_auto_open_close(done) {
    let highlight = document.getElementById("UITourHighlight");
    gContentAPI.showHighlight("customize");
    waitForElementToBeVisible(highlight, function checkPanelIsOpen() {
      isnot(PanelUI.panel.state, "closed", "Panel should have opened");
      isnot(highlight.classList.contains("rounded-highlight"), true, "Highlight should not be round-rectangle styled.");

      let hiddenPromise = promisePanelElementHidden(window, PanelUI.panel);
      // Move the highlight outside which should close the app menu.
      gContentAPI.showHighlight("appMenu");
      hiddenPromise.then(() => {
        waitForElementToBeVisible(highlight, function checkPanelIsClosed() {
          isnot(PanelUI.panel.state, "open",
                "Panel should have closed after the highlight moved elsewhere.");
          done();
        }, "Highlight should move to the appMenu button");
      });
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
        waitForElementToBeVisible(highlight, function() {
          isnot(PanelUI.panel.state, "closed",
                "Panel should remain open since UITour didn't open it in the first place");
          gContentAPI.hideMenu("appMenu");
          done();
        }, "Highlight should move to the appMenu button");
      }, "Highlight should be shown after showHighlight() for fixed panel items");
    }).catch(Cu.reportError);
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
        highlight.addEventListener("animationstart", function(aEvent) {
          ok(true, "Animation occurred again even though the effect was the same");
          checkRandomEffect();
        }, {once: true});
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

    popup.addEventListener("popupshown", function() {
      is(popup.anchorNode, document.getElementById("urlbar"), "Popup should be anchored to the urlbar");
      is(title.textContent, "test title", "Popup should have correct title");
      is(desc.textContent, "test text", "Popup should have correct description text");
      is(icon.src, "", "Popup should have no icon");
      is(buttons.hasChildNodes(), false, "Popup should have no buttons");

      popup.addEventListener("popuphidden", function() {
        popup.addEventListener("popupshown", function() {
          done();
        }, {once: true});

        gContentAPI.showInfo("urlbar", "test title", "test text");

      }, {once: true});
      gContentAPI.hideInfo();
    }, {once: true});

    gContentAPI.showInfo("urlbar", "test title", "test text");
  },
  taskify(async function test_info_2() {
    let popup = document.getElementById("UITourTooltip");
    let title = document.getElementById("UITourTooltipTitle");
    let desc = document.getElementById("UITourTooltipDescription");
    let icon = document.getElementById("UITourTooltipIcon");
    let buttons = document.getElementById("UITourTooltipButtons");

    await showInfoPromise("urlbar", "urlbar title", "urlbar text");

    is(popup.anchorNode, document.getElementById("urlbar"), "Popup should be anchored to the urlbar");
    is(title.textContent, "urlbar title", "Popup should have correct title");
    is(desc.textContent, "urlbar text", "Popup should have correct description text");
    is(icon.src, "", "Popup should have no icon");
    is(buttons.hasChildNodes(), false, "Popup should have no buttons");

    // Place the search bar in the navigation toolbar temporarily.
    await SpecialPowers.pushPrefEnv({ set: [
      ["browser.search.widget.inNavBar", true],
    ]});

    await showInfoPromise("search", "search title", "search text");

    is(popup.anchorNode, document.getElementById("searchbar"), "Popup should be anchored to the searchbar");
    is(title.textContent, "search title", "Popup should have correct title");
    is(desc.textContent, "search text", "Popup should have correct description text");

    await SpecialPowers.popPrefEnv();
  }),
  function test_getConfigurationVersion(done) {
    function callback(result) {
      ok(typeof result.version !== "undefined", "Check version isn't undefined.");
      is(result.version, Services.appinfo.version, "Should have the same version property.");
      is(result.defaultUpdateChannel, UpdateUtils.getUpdateChannel(false), "Should have the correct update channel.");
      done();
    }

    gContentAPI.getConfiguration("appinfo", callback);
  },
  function test_getConfigurationDistribution(done) {
    gContentAPI.getConfiguration("appinfo", (result) => {
      ok(typeof(result.distribution) !== "undefined", "Check distribution isn't undefined.");
      is(result.distribution, "default", "Should be \"default\" without preference set.");

      let defaults = Services.prefs.getDefaultBranch("distribution.");
      let testDistributionID = "TestDistribution";
      defaults.setCharPref("id", testDistributionID);
      gContentAPI.getConfiguration("appinfo", (result2) => {
        ok(typeof(result2.distribution) !== "undefined", "Check distribution isn't undefined.");
        is(result2.distribution, testDistributionID, "Should have the distribution as set in preference.");

        done();
      });
    });
  },
  function test_getConfigurationProfileAge(done) {
    gContentAPI.getConfiguration("appinfo", (result) => {
      ok(typeof(result.profileCreatedWeeksAgo) === "number", "profileCreatedWeeksAgo should be number.");
      ok(result.profileResetWeeksAgo === null, "profileResetWeeksAgo should be null.");

      // Set profile reset date to 15 days ago.
      let profileAccessor = new ProfileAge();
      profileAccessor.recordProfileReset(Date.now() - (15 * 24 * 60 * 60 * 1000));
      gContentAPI.getConfiguration("appinfo", (result2) => {
        ok(typeof(result2.profileResetWeeksAgo) === "number", "profileResetWeeksAgo should be number.");
        is(result2.profileResetWeeksAgo, 2, "profileResetWeeksAgo should be 2.");
        done();
      });
    });
  },
  function test_addToolbarButton(done) {
    let placement = CustomizableUI.getPlacementOfWidget("panic-button");
    is(placement, null, "default UI has panic button in the palette");

    gContentAPI.getConfiguration("availableTargets", (data) => {
      let available = (data.targets.includes("forget"));
      ok(!available, "Forget button should not be available by default");

      gContentAPI.addNavBarWidget("forget", () => {
        info("addNavBarWidget callback successfully called");

        let updatedPlacement = CustomizableUI.getPlacementOfWidget("panic-button");
        is(updatedPlacement.area, CustomizableUI.AREA_NAVBAR);

        gContentAPI.getConfiguration("availableTargets", (data2) => {
          let updatedAvailable = data2.targets.includes("forget");
          ok(updatedAvailable, "Forget button should now be available");

          // Cleanup
          CustomizableUI.removeWidgetFromArea("panic-button");
          done();
        });
      });
    });
  },
  function test_search(done) {
    Services.search.init(rv => {
      if (!Components.isSuccessCode(rv)) {
        ok(false, "search service init failed: " + rv);
        done();
        return;
      }
      let defaultEngine = Services.search.defaultEngine;
      gContentAPI.getConfiguration("search", data => {
        let visibleEngines = Services.search.getVisibleEngines();
        let expectedEngines = visibleEngines.filter((engine) => engine.identifier)
                                            .map((engine) => "searchEngine-" + engine.identifier);

        let engines = data.engines;
        ok(Array.isArray(engines), "data.engines should be an array");
        is(engines.sort().toString(), expectedEngines.sort().toString(),
           "Engines should be as expected");

        is(data.searchEngineIdentifier, defaultEngine.identifier,
           "the searchEngineIdentifier property should contain the defaultEngine's identifier");

        let someOtherEngineID = data.engines.filter(t => t != "searchEngine-" + defaultEngine.identifier)[0];
        someOtherEngineID = someOtherEngineID.replace(/^searchEngine-/, "");

        let observe = function(subject, topic, verb) {
          info("browser-search-engine-modified: " + verb);
          if (verb == "engine-current") {
            is(Services.search.defaultEngine.identifier, someOtherEngineID, "correct engine was switched to");
            done();
          }
        };
        Services.obs.addObserver(observe, "browser-search-engine-modified");
        registerCleanupFunction(() => {
          // Clean up
          Services.obs.removeObserver(observe, "browser-search-engine-modified");
          Services.search.defaultEngine = defaultEngine;
        });

        gContentAPI.setDefaultSearchEngine(someOtherEngineID);
      });
    });
  },
  taskify(async function test_treatment_tag() {
    let ac = new TelemetryArchiveTesting.Checker();
    await ac.promiseInit();
    await gContentAPI.setTreatmentTag("foobar", "baz");
    // Wait until the treatment telemetry is sent before looking in the archive.
    await BrowserTestUtils.waitForContentEvent(gTestTab.linkedBrowser, "mozUITourNotification", false,
                                               event => event.detail.event === "TreatmentTag:TelemetrySent");
    await new Promise((resolve) => {
      gContentAPI.getTreatmentTag("foobar", (data) => {
        is(data.value, "baz", "set and retrieved treatmentTag");
        ac.promiseFindPing("uitour-tag", [
          [["payload", "tagName"], "foobar"],
          [["payload", "tagValue"], "baz"],
        ]).then((found) => {
          ok(found, "Telemetry ping submitted for setTreatmentTag");
          resolve();
        }, (err) => {
          ok(false, "Exception finding uitour telemetry ping: " + err);
          resolve();
        });
      });
    });
  }),

  // Make sure this test is last in the file so the appMenu gets left open and done will confirm it got tore down.
  taskify(async function cleanupMenus() {
    let shownPromise = promisePanelShown(window);
    gContentAPI.showMenu("appMenu");
    await shownPromise;
  }),
];
