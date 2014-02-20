/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that annotations disappear when their target is hidden.
 */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;
let gContentDoc;
let highlight = document.getElementById("UITourHighlight");
let tooltip = document.getElementById("UITourTooltip");

Components.utils.import("resource:///modules/UITour.jsm");

function test() {
  registerCleanupFunction(function() {
    gContentDoc = null;
  });
  UITourTest();
}

let tests = [
  function test_move_tab_to_new_window(done) {
    let gOpenedWindow;
    let onVisibilityChange = (aEvent) => {
      if (!document.hidden && window != UITour.getChromeWindow(aEvent.target)) {
        gContentAPI.showHighlight("appMenu");
      }
    };
    let onDOMWindowDestroyed = (aWindow, aTopic, aData) => {
      if (gOpenedWindow && aWindow == gOpenedWindow) {
        Services.obs.removeObserver(onDOMWindowDestroyed, "dom-window-destroyed", false);
        done();
      }
    };
    let onBrowserDelayedStartup = (aWindow, aTopic, aData) => {
      gOpenedWindow = aWindow;
      Services.obs.removeObserver(onBrowserDelayedStartup, "browser-delayed-startup-finished");
      try {
        let newWindowHighlight = gOpenedWindow.document.getElementById("UITourHighlight");
        let selectedTab = aWindow.gBrowser.selectedTab;
        is(selectedTab.linkedBrowser && selectedTab.linkedBrowser.contentDocument, gContentDoc, "Document should be selected in new window");
        ok(UITour.originTabs && UITour.originTabs.has(aWindow), "Window should be known");
        ok(UITour.originTabs.get(aWindow).has(selectedTab), "Tab should be known");
        waitForElementToBeVisible(newWindowHighlight, function checkHighlightIsThere() {
          gContentAPI.showMenu("appMenu");
          isnot(aWindow.PanelUI.panel.state, "closed", "Panel should be open");
          ok(aWindow.PanelUI.contents.children.length > 0, "Panel contents should have children");
          gContentAPI.hideHighlight();
          gContentAPI.hideMenu("appMenu");
          gTestTab = null;
          aWindow.close();
        }, "Highlight should be shown in new window.");
      } catch (ex) {
        Cu.reportError(ex);
        ok(false, "An error occurred running UITour tab detach test.");
      } finally {
        gContentDoc.removeEventListener("visibilitychange", onVisibilityChange, false);
        Services.obs.addObserver(onDOMWindowDestroyed, "dom-window-destroyed", false);
      }
    };

    Services.obs.addObserver(onBrowserDelayedStartup, "browser-delayed-startup-finished", false);
    // NB: we're using this rather than gContentWindow.document because the latter wouldn't
    // have an XRayWrapper, and we need to compare this to the doc we get using this method
    // later on...
    gContentDoc = gBrowser.selectedTab.linkedBrowser.contentDocument;
    gContentDoc.addEventListener("visibilitychange", onVisibilityChange, false);
    gContentAPI.showHighlight("appMenu");
    waitForElementToBeVisible(highlight, function checkForInitialHighlight() {
      gBrowser.replaceTabWithWindow(gBrowser.selectedTab);
    });

  },
];

