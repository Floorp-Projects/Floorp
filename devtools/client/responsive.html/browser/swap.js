/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const promise = require("promise");
const { Task } = require("devtools/shared/task");
const { tunnelToInnerBrowser } = require("./tunnel");

/**
 * Swap page content from an existing tab into a new browser within a container
 * page.  Page state is preserved by using `swapFrameLoaders`, just like when
 * you move a tab to a new window.  This provides a seamless transition for the
 * user since the page is not reloaded.
 *
 * See /devtools/docs/responsive-design-mode.md for a high level overview of how
 * this is used in RDM.  The steps described there are copied into the code
 * below.
 *
 * For additional low level details about swapping browser content,
 * see /devtools/client/responsive.html/docs/browser-swap.md.
 *
 * @param tab
 *        A browser tab with content to be swapped.
 * @param containerURL
 *        URL to a page that holds an inner browser.
 * @param getInnerBrowser
 *        Function that returns a Promise to the inner browser within the
 *        container page.  It is called with the outer browser that loaded the
 *        container page.
 */
function swapToInnerBrowser({ tab, containerURL, getInnerBrowser }) {
  let gBrowser = tab.ownerDocument.defaultView.gBrowser;
  let innerBrowser;
  let tunnel;

  // Dispatch a custom event each time the _viewport content_ is swapped from one browser
  // to another.  DevTools server code uses this to follow the content if there is an
  // active DevTools connection.  While browser.xml does dispatch it's own SwapDocShells
  // event, this one is easier for DevTools to follow because it's only emitted once per
  // transition, instead of twice like SwapDocShells.
  let dispatchDevToolsBrowserSwap = (from, to) => {
    let CustomEvent = tab.ownerDocument.defaultView.CustomEvent;
    let event = new CustomEvent("DevTools:BrowserSwap", {
      detail: to,
      bubbles: true,
    });
    from.dispatchEvent(event);
  };

  return {

    start: Task.async(function* () {
      tab.isResponsiveDesignMode = true;

      // Hide the browser content temporarily while things move around to avoid displaying
      // strange intermediate states.
      tab.linkedBrowser.style.visibility = "hidden";

      // Freeze navigation temporarily to avoid "blinking" in the location bar.
      freezeNavigationState(tab);

      // 1. Create a temporary, hidden tab to load the tool UI.
      let containerTab = gBrowser.addTab("about:blank", {
        skipAnimation: true,
        forceNotRemote: true,
      });
      gBrowser.hideTab(containerTab);
      let containerBrowser = containerTab.linkedBrowser;
      // Even though we load the `containerURL` with `LOAD_FLAGS_BYPASS_HISTORY` below,
      // `SessionHistory.jsm` has a fallback path for tabs with no history which
      // fabricates a history entry by reading the current URL, and this can cause the
      // container URL to be recorded in the session store.  To avoid this, we send a
      // bogus `epoch` value to our container tab, which causes all future history
      // messages to be ignored.  (Actual navigations are still correctly recorded because
      // this only affects the container frame, not the content.)  A better fix would be
      // to just not load the `content-sessionStore.js` frame script at all in the
      // container tab, but it's loaded for all tab browsers, so this seems a bit harder
      // to achieve in a nice way.
      containerBrowser.messageManager.sendAsyncMessage("SessionStore:flush", {
        epoch: -1,
      });
      // Prevent the `containerURL` from ending up in the tab's history.
      containerBrowser.loadURIWithFlags(containerURL, {
        flags: Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY,
      });

      // Copy tab listener state flags to container tab.  Each tab gets its own tab
      // listener and state flags which cache document loading progress.  The state flags
      // are checked when switching tabs to update the browser UI.  The later step of
      // `swapBrowsersAndCloseOther` will fold the state back into the main tab.
      let stateFlags = gBrowser._tabListeners.get(tab).mStateFlags;
      gBrowser._tabListeners.get(containerTab).mStateFlags = stateFlags;

      // 2. Mark the tool tab browser's docshell as active so the viewport frame
      //    is created eagerly and will be ready to swap.
      // This line is crucial when the tool UI is loaded into a background tab.
      // Without it, the viewport browser's frame is created lazily, leading to
      // a multi-second delay before it would be possible to `swapFrameLoaders`.
      // Even worse than the delay, there appears to be no obvious event fired
      // after the frame is set lazily, so it's unclear how to know that work
      // has finished.
      containerBrowser.docShellIsActive = true;

      // 3. Create the initial viewport inside the tool UI.
      // The calling application will use container page loaded into the tab to
      // do whatever it needs to create the inner browser.
      yield tabLoaded(containerTab);
      innerBrowser = yield getInnerBrowser(containerBrowser);
      addXULBrowserDecorations(innerBrowser);
      if (innerBrowser.isRemoteBrowser != tab.linkedBrowser.isRemoteBrowser) {
        throw new Error("The inner browser's remoteness must match the " +
                        "original tab.");
      }

      // 4. Swap tab content from the regular browser tab to the browser within
      //    the viewport in the tool UI, preserving all state via
      //    `gBrowser._swapBrowserDocShells`.
      dispatchDevToolsBrowserSwap(tab.linkedBrowser, innerBrowser);
      gBrowser._swapBrowserDocShells(tab, innerBrowser);

      // 5. Force the original browser tab to be non-remote since the tool UI
      //    must be loaded in the parent process, and we're about to swap the
      //    tool UI into this tab.
      gBrowser.updateBrowserRemoteness(tab.linkedBrowser, false);

      // 6. Swap the tool UI (with viewport showing the content) into the
      //    original browser tab and close the temporary tab used to load the
      //    tool via `swapBrowsersAndCloseOther`.
      gBrowser.swapBrowsersAndCloseOther(tab, containerTab);

      // 7. Start a tunnel from the tool tab's browser to the viewport browser
      //    so that some browser UI functions, like navigation, are connected to
      //    the content in the viewport, instead of the tool page.
      tunnel = tunnelToInnerBrowser(tab.linkedBrowser, innerBrowser);
      yield tunnel.start();

      // Swapping browsers disconnects the find bar UI from the browser.
      // If the find bar has been initialized, reconnect it.
      if (gBrowser.isFindBarInitialized(tab)) {
        let findBar = gBrowser.getFindBar(tab);
        findBar.browser = tab.linkedBrowser;
        if (!findBar.hidden) {
          // Force the find bar to activate again, restoring the search string.
          findBar.onFindCommand();
        }
      }

      // Force the browser UI to match the new state of the tab and browser.
      thawNavigationState(tab);
      gBrowser.setTabTitle(tab);
      gBrowser.updateCurrentBrowser(true);

      // Show the browser content again now that the move is done.
      tab.linkedBrowser.style.visibility = "";
    }),

    stop() {
      // Hide the browser content temporarily while things move around to avoid displaying
      // strange intermediate states.
      tab.linkedBrowser.style.visibility = "hidden";

      // 1. Stop the tunnel between outer and inner browsers.
      tunnel.stop();
      tunnel = null;

      // 2. Create a temporary, hidden tab to hold the content.
      let contentTab = gBrowser.addTab("about:blank", {
        skipAnimation: true,
      });
      gBrowser.hideTab(contentTab);
      let contentBrowser = contentTab.linkedBrowser;

      // 3. Mark the content tab browser's docshell as active so the frame
      //    is created eagerly and will be ready to swap.
      contentBrowser.docShellIsActive = true;

      // 4. Swap tab content from the browser within the viewport in the tool UI
      //    to the regular browser tab, preserving all state via
      //    `gBrowser._swapBrowserDocShells`.
      dispatchDevToolsBrowserSwap(innerBrowser, contentBrowser);
      gBrowser._swapBrowserDocShells(contentTab, innerBrowser);
      innerBrowser = null;

      // Copy tab listener state flags to content tab.  See similar comment in `start`
      // above for more details.
      let stateFlags = gBrowser._tabListeners.get(tab).mStateFlags;
      gBrowser._tabListeners.get(contentTab).mStateFlags = stateFlags;

      // 5. Force the original browser tab to be remote since web content is
      //    loaded in the child process, and we're about to swap the content
      //    into this tab.
      gBrowser.updateBrowserRemoteness(tab.linkedBrowser, true, {
        remoteType: contentBrowser.remoteType,
      });

      // 6. Swap the content into the original browser tab and close the
      //    temporary tab used to hold the content via
      //    `swapBrowsersAndCloseOther`.
      dispatchDevToolsBrowserSwap(contentBrowser, tab.linkedBrowser);
      gBrowser.swapBrowsersAndCloseOther(tab, contentTab);

      // Swapping browsers disconnects the find bar UI from the browser.
      // If the find bar has been initialized, reconnect it.
      if (gBrowser.isFindBarInitialized(tab)) {
        let findBar = gBrowser.getFindBar(tab);
        findBar.browser = tab.linkedBrowser;
        if (!findBar.hidden) {
          // Force the find bar to activate again, restoring the search string.
          findBar.onFindCommand();
        }
      }

      gBrowser = null;

      // The focus manager seems to get a little dizzy after all this swapping.  If a
      // content element had been focused inside the viewport before stopping, it will
      // have lost focus.  Activate the frame to restore expected focus.
      tab.linkedBrowser.frameLoader.activateRemoteFrame();

      delete tab.isResponsiveDesignMode;

      // Show the browser content again now that the move is done.
      tab.linkedBrowser.style.visibility = "";
    },

  };
}

/**
 * Browser navigation properties we'll freeze temporarily to avoid "blinking" in the
 * location bar, etc. caused by the containerURL peeking through before the swap is
 * complete.
 */
const NAVIGATION_PROPERTIES = [
  "currentURI",
  "contentTitle",
  "securityUI",
];

function freezeNavigationState(tab) {
  // Browser navigation properties we'll freeze temporarily to avoid "blinking" in the
  // location bar, etc. caused by the containerURL peeking through before the swap is
  // complete.
  for (let property of NAVIGATION_PROPERTIES) {
    let value = tab.linkedBrowser[property];
    Object.defineProperty(tab.linkedBrowser, property, {
      get() {
        return value;
      },
      configurable: true,
      enumerable: true,
    });
  }
}

function thawNavigationState(tab) {
  // Thaw out the properties we froze at the beginning now that the swap is complete.
  for (let property of NAVIGATION_PROPERTIES) {
    delete tab.linkedBrowser[property];
  }
}

/**
 * Browser elements that are passed to `gBrowser._swapBrowserDocShells` are
 * expected to have certain properties that currently exist only on
 * <xul:browser> elements.  In particular, <iframe mozbrowser> elements don't
 * have them.
 *
 * Rather than duplicate the swapping code used by the browser to work around
 * this, we stub out the missing properties needed for the swap to complete.
 */
function addXULBrowserDecorations(browser) {
  if (browser.isRemoteBrowser == undefined) {
    Object.defineProperty(browser, "isRemoteBrowser", {
      get() {
        return this.getAttribute("remote") == "true";
      },
      configurable: true,
      enumerable: true,
    });
  }
  if (browser.remoteType == undefined) {
    Object.defineProperty(browser, "remoteType", {
      get() {
        return this.getAttribute("remoteType");
      },
      configurable: true,
      enumerable: true,
    });
  }
  if (browser.messageManager == undefined) {
    Object.defineProperty(browser, "messageManager", {
      get() {
        return this.frameLoader.messageManager;
      },
      configurable: true,
      enumerable: true,
    });
  }
  if (browser.outerWindowID == undefined) {
    Object.defineProperty(browser, "outerWindowID", {
      get() {
        return browser._outerWindowID;
      },
      configurable: true,
      enumerable: true,
    });
  }

  // It's not necessary for these to actually do anything.  These properties are
  // swapped between browsers in browser.xml's `swapDocShells`, and then their
  // `swapBrowser` methods are called, so we define them here for that to work
  // without errors.  During the swap process above, these will move from the
  // the new inner browser to the original tab's browser (step 4) and then to
  // the temporary container tab's browser (step 7), which is then closed.
  if (browser._remoteWebNavigationImpl == undefined) {
    browser._remoteWebNavigationImpl = {
      swapBrowser() {},
    };
  }
  if (browser._remoteWebProgressManager == undefined) {
    browser._remoteWebProgressManager = {
      swapBrowser() {},
    };
  }
}

function tabLoaded(tab) {
  let deferred = promise.defer();

  function handle(event) {
    if (event.originalTarget != tab.linkedBrowser.contentDocument ||
        event.target.location.href == "about:blank") {
      return;
    }
    tab.linkedBrowser.removeEventListener("load", handle, true);
    deferred.resolve(event);
  }

  tab.linkedBrowser.addEventListener("load", handle, true);
  return deferred.promise;
}

exports.swapToInnerBrowser = swapToInnerBrowser;
