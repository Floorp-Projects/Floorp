/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Detaching a tab to a new window shouldn't break the menu panel.
 */

"use strict";

var gTestTab;
var gContentAPI;
var gContentDoc;

var detachedWindow;

function test() {
  registerCleanupFunction(function() {
    gContentDoc = null;
  });
  UITourTest();
}

/**
 * When tab is changed we're tearing the tour down. So the UITour client has to always be aware of this
 * fact and therefore listens to pageshow events.
 * In particular this scenario happens for detaching the tab (ie. moving it to a new window).
 */
var tests = [
  taskify(async function test_move_tab_to_new_window() {
    const myDocIdentifier =
      "Hello, I'm a unique expando to identify this document.";

    let highlight = document.getElementById("UITourHighlight");

    let browserStartupDeferred = PromiseUtils.defer();
    Services.obs.addObserver(function onBrowserDelayedStartup(aWindow) {
      Services.obs.removeObserver(
        onBrowserDelayedStartup,
        "browser-delayed-startup-finished"
      );
      browserStartupDeferred.resolve(aWindow);
    }, "browser-delayed-startup-finished");

    await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [myDocIdentifier],
      contentMyDocIdentifier => {
        let onPageShow = () => {
          if (!content.document.hidden) {
            let win = Cu.waiveXrays(content);
            win.Mozilla.UITour.showHighlight("appMenu");
          }
        };
        content.window.addEventListener("pageshow", onPageShow, {
          mozSystemGroup: true,
        });
        content.document.myExpando = contentMyDocIdentifier;
      }
    );
    gContentAPI.showHighlight("appMenu");

    await elementVisiblePromise(highlight, "old window highlight");

    detachedWindow = gBrowser.replaceTabWithWindow(gBrowser.selectedTab);
    await browserStartupDeferred.promise;

    // This highlight should be shown thanks to the pageshow listener.
    let newWindowHighlight = UITour.getHighlightAndMaybeCreate(
      detachedWindow.document
    );
    await elementVisiblePromise(newWindowHighlight, "new window highlight");

    let selectedTab = detachedWindow.gBrowser.selectedTab;
    await SpecialPowers.spawn(
      selectedTab.linkedBrowser,
      [myDocIdentifier],
      contentMyDocIdentifier => {
        is(
          content.document.myExpando,
          contentMyDocIdentifier,
          "Document should be selected in new window"
        );
      }
    );
    ok(
      UITour.tourBrowsersByWindow &&
        UITour.tourBrowsersByWindow.has(detachedWindow),
      "Window should be known"
    );
    ok(
      UITour.tourBrowsersByWindow
        .get(detachedWindow)
        .has(selectedTab.linkedBrowser),
      "Selected browser should be known"
    );

    // Need this because gContentAPI in e10s land will try to use gTestTab to
    // spawn a content task, which doesn't work if the tab is dead, for obvious
    // reasons.
    gTestTab = detachedWindow.gBrowser.selectedTab;

    let shownPromise = promisePanelShown(detachedWindow);
    gContentAPI.showMenu("appMenu");
    await shownPromise;

    isnot(detachedWindow.PanelUI.panel.state, "closed", "Panel should be open");
    gContentAPI.hideHighlight();
    gContentAPI.hideMenu("appMenu");
    gTestTab = null;

    await BrowserTestUtils.closeWindow(detachedWindow);
  }),
];
