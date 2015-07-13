/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Detaching a tab to a new window shouldn't break the menu panel.
 */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;
let gContentDoc;

function test() {
  registerCleanupFunction(function() {
    gContentDoc = null;
  });
  UITourTest();
}

/**
 * When tab is changed we're tearing the tour down. So the UITour client has to always be aware of this
 * fact and therefore listens to visibilitychange events.
 * In particular this scenario happens for detaching the tab (ie. moving it to a new window).
 */
let tests = [
  taskify(function* test_move_tab_to_new_window(done) {
    let onVisibilityChange = (aEvent) => {
      if (!document.hidden && window != UITour.getChromeWindow(aEvent.target)) {
        gContentAPI.showHighlight("appMenu");
      }
    };

    let highlight = document.getElementById("UITourHighlight");
    let windowDestroyedDeferred = Promise.defer();
    let onDOMWindowDestroyed = (aWindow) => {
      if (gContentWindow && aWindow == gContentWindow) {
        Services.obs.removeObserver(onDOMWindowDestroyed, "dom-window-destroyed", false);
        windowDestroyedDeferred.resolve();
      }
    };

    let browserStartupDeferred = Promise.defer();
    Services.obs.addObserver(function onBrowserDelayedStartup(aWindow) {
      Services.obs.removeObserver(onBrowserDelayedStartup, "browser-delayed-startup-finished");
      browserStartupDeferred.resolve(aWindow);
    }, "browser-delayed-startup-finished", false);

    // NB: we're using this rather than gContentWindow.document because the latter wouldn't
    // have an XRayWrapper, and we need to compare this to the doc we get using this method
    // later on...
    gContentDoc = gBrowser.selectedBrowser.contentDocument;
    gContentDoc.addEventListener("visibilitychange", onVisibilityChange, false);
    gContentAPI.showHighlight("appMenu");

    yield elementVisiblePromise(highlight);

    gBrowser.replaceTabWithWindow(gBrowser.selectedTab);

    gContentWindow = yield browserStartupDeferred.promise;

    // This highlight should be shown thanks to the visibilitychange listener.
    let newWindowHighlight = gContentWindow.document.getElementById("UITourHighlight");
    yield elementVisiblePromise(newWindowHighlight);

    let selectedTab = gContentWindow.gBrowser.selectedTab;
    is(selectedTab.linkedBrowser && selectedTab.linkedBrowser.contentDocument, gContentDoc, "Document should be selected in new window");
    ok(UITour.tourBrowsersByWindow && UITour.tourBrowsersByWindow.has(gContentWindow), "Window should be known");
    ok(UITour.tourBrowsersByWindow.get(gContentWindow).has(selectedTab.linkedBrowser), "Selected browser should be known");

    let shownPromise = promisePanelShown(gContentWindow);
    gContentAPI.showMenu("appMenu");
    yield shownPromise;

    isnot(gContentWindow.PanelUI.panel.state, "closed", "Panel should be open");
    ok(gContentWindow.PanelUI.contents.children.length > 0, "Panel contents should have children");
    gContentAPI.hideHighlight();
    gContentAPI.hideMenu("appMenu");
    gTestTab = null;

    Services.obs.addObserver(onDOMWindowDestroyed, "dom-window-destroyed", false);
    gContentWindow.close();

    yield windowDestroyedDeferred.promise;
  }),
];
