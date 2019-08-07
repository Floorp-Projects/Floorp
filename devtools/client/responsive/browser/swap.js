/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const Services = require("Services");
const { E10SUtils } = require("resource://gre/modules/E10SUtils.jsm");
const { tunnelToInnerBrowser } = require("./tunnel");

function debug(msg) {
  // console.log(`RDM swap: ${msg}`);
}

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
  let browserWindow = tab.ownerGlobal;
  let gBrowser = browserWindow.gBrowser;
  let innerBrowser;
  let tunnel;

  // Dispatch a custom event each time the _viewport content_ is swapped from one browser
  // to another.  DevTools server code uses this to follow the content if there is an
  // active DevTools connection.  While browser.js does dispatch it's own SwapDocShells
  // event, this one is easier for DevTools to follow because it's only emitted once per
  // transition, instead of twice like SwapDocShells.
  const dispatchDevToolsBrowserSwap = (from, to) => {
    const CustomEvent = browserWindow.CustomEvent;
    const event = new CustomEvent("DevTools:BrowserSwap", {
      detail: to,
      bubbles: true,
    });
    from.dispatchEvent(event);
  };

  // A version of `gBrowser.addTab` that absorbs the `TabOpen` event.
  // The swap process uses a temporary tab, and there's no real need for others to hear
  // about it.  This hides the temporary tab from things like WebExtensions.
  const addTabSilently = (uri, options) => {
    browserWindow.addEventListener(
      "TabOpen",
      event => {
        event.stopImmediatePropagation();
      },
      { capture: true, once: true }
    );
    options.triggeringPrincipal = Services.scriptSecurityManager.createNullPrincipal(
      {
        userContextId: options.userContextId,
      }
    );
    return gBrowser.addWebTab(uri, options);
  };

  // A version of `gBrowser.swapBrowsersAndCloseOther` that absorbs the `TabClose` event.
  // The swap process uses a temporary tab, and there's no real need for others to hear
  // about it.  This hides the temporary tab from things like WebExtensions.
  const swapBrowsersAndCloseOtherSilently = (ourTab, otherTab) => {
    browserWindow.addEventListener(
      "TabClose",
      event => {
        event.stopImmediatePropagation();
      },
      { capture: true, once: true }
    );
    gBrowser.swapBrowsersAndCloseOther(ourTab, otherTab);
  };

  // It is possible for the frame loader swap within `gBrowser._swapBrowserDocShells` to
  // fail when various frame state is either not ready yet or doesn't match between the
  // two browsers you're trying to swap.  However, such errors are currently caught and
  // silenced in the browser, because they are apparently expected in certain cases.
  // So, here we do our own check to verify that the swap actually did in fact take place,
  // making it much easier to track such errors when they happen.
  const swapBrowserDocShells = (ourTab, otherBrowser) => {
    // The verification step here assumes both browsers are remote.
    if (
      !ourTab.linkedBrowser.isRemoteBrowser ||
      !otherBrowser.isRemoteBrowser
    ) {
      throw new Error("Both browsers should be remote before swapping.");
    }
    const contentTabId = ourTab.linkedBrowser.frameLoader.remoteTab.tabId;
    gBrowser._swapBrowserDocShells(ourTab, otherBrowser);
    if (otherBrowser.frameLoader.remoteTab.tabId != contentTabId) {
      // Bug 1408602: Try to unwind to save tab content from being lost.
      throw new Error("Swapping tab content between browsers failed.");
    }
  };

  // Wait for a browser to load into a new frame loader.
  function loadURIWithNewFrameLoader(browser, uri, options) {
    return new Promise(resolve => {
      gBrowser.addEventListener("XULFrameLoaderCreated", resolve, {
        once: true,
      });
      browser.loadURI(uri, options);
    });
  }

  return {
    async start() {
      // In some cases, such as a preloaded browser used for about:newtab, browser code
      // will force a new frameloader on next navigation to remote content to ensure
      // balanced process assignment.  If this case will happen here, navigate to
      // about:blank first to get this out of way so that we stay within one process while
      // RDM is open. Some process selection rules are specific to remote content, so we
      // use `http://example.com` as a test for what a remote navigation would cause.
      const {
        requiredRemoteType,
        mustChangeProcess,
        newFrameloader,
      } = E10SUtils.shouldLoadURIInBrowser(
        tab.linkedBrowser,
        "http://example.com"
      );
      if (newFrameloader) {
        debug(
          `Tab will force a new frameloader on navigation, load about:blank first`
        );
        await loadURIWithNewFrameLoader(tab.linkedBrowser, "about:blank", {
          flags: Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY,
          triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
            {}
          ),
        });
      }
      // When the separate privileged content process is enabled, about:home and
      // about:newtab will load in it, and we'll need to switch away if the user
      // ever browses to a new URL. To avoid that, when the privileged process is
      // enabled, we do the process flip immediately before entering RDM mode. The
      // trade-off is that about:newtab can't be inspected in RDM, but it allows
      // users to start RDM on that page and keep it open.
      //
      // The other trade is that sometimes users will be viewing the local file
      // URI process, and will want to view the page in RDM. We allow this without
      // blanking out the page, but we trade that for closing RDM if browsing ever
      // causes them to flip processes.
      //
      // Bug 1510806 has been filed to fix this properly, by making RDM resilient
      // to process flips.
      if (
        mustChangeProcess &&
        tab.linkedBrowser.remoteType == "privilegedabout"
      ) {
        debug(
          `Tab must flip away from the privileged content process ` +
            `on navigation`
        );
        gBrowser.updateBrowserRemoteness(tab.linkedBrowser, {
          remoteType: requiredRemoteType,
        });
      }

      tab.isResponsiveDesignMode = true;

      // Hide the browser content temporarily while things move around to avoid displaying
      // strange intermediate states.
      tab.linkedBrowser.style.visibility = "hidden";

      // Freeze navigation temporarily to avoid "blinking" in the location bar.
      freezeNavigationState(tab);

      // 1. Create a temporary, hidden tab to load the tool UI.
      debug("Add blank tool tab");
      const containerTab = addTabSilently("about:blank", {
        skipAnimation: true,
        forceNotRemote: true,
        userContextId: tab.userContextId,
      });
      gBrowser.hideTab(containerTab);
      const containerBrowser = containerTab.linkedBrowser;
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
      debug("Load container URL");
      containerBrowser.loadURI(containerURL, {
        flags: Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY,
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });

      // Copy tab listener state flags to container tab.  Each tab gets its own tab
      // listener and state flags which cache document loading progress.  The state flags
      // are checked when switching tabs to update the browser UI.  The later step of
      // `swapBrowsersAndCloseOther` will fold the state back into the main tab.
      const stateFlags = gBrowser._tabListeners.get(tab).mStateFlags;
      gBrowser._tabListeners.get(containerTab).mStateFlags = stateFlags;

      // 2. Mark the tool tab browser's docshell as active so the viewport frame
      //    is created eagerly and will be ready to swap.
      // This line is crucial when the tool UI is loaded into a background tab.
      // Without it, the viewport browser's frame is created lazily, leading to
      // a multi-second delay before it would be possible to `swapFrameLoaders`.
      // Even worse than the delay, there appears to be no obvious event fired
      // after the frame is set lazily, so it's unclear how to know that work
      // has finished.
      debug("Set container docShell active");
      containerBrowser.docShellIsActive = true;

      // 3. Create the initial viewport inside the tool UI.
      // The calling application will use container page loaded into the tab to
      // do whatever it needs to create the inner browser.
      debug("Wait until container tab loaded");
      await tabLoaded(containerTab);
      debug("Wait until inner browser available");
      innerBrowser = await getInnerBrowser(containerBrowser);
      addXULBrowserDecorations(innerBrowser);
      if (innerBrowser.isRemoteBrowser != tab.linkedBrowser.isRemoteBrowser) {
        throw new Error(
          "The inner browser's remoteness must match the " + "original tab."
        );
      }

      // 4. Swap tab content from the regular browser tab to the browser within
      //    the viewport in the tool UI, preserving all state via
      //    `gBrowser._swapBrowserDocShells`.
      dispatchDevToolsBrowserSwap(tab.linkedBrowser, innerBrowser);
      debug("Swap content to inner browser");
      swapBrowserDocShells(tab, innerBrowser);

      // 5. Force the original browser tab to be non-remote since the tool UI
      //    must be loaded in the parent process, and we're about to swap the
      //    tool UI into this tab.
      debug("Flip original tab to remote false");
      gBrowser.updateBrowserRemoteness(tab.linkedBrowser, {
        remoteType: E10SUtils.NOT_REMOTE,
      });

      // 6. Swap the tool UI (with viewport showing the content) into the
      //    original browser tab and close the temporary tab used to load the
      //    tool via `swapBrowsersAndCloseOther`.
      debug("Swap tool UI to original tab");
      swapBrowsersAndCloseOtherSilently(tab, containerTab);

      // 7. Start a tunnel from the tool tab's browser to the viewport browser
      //    so that some browser UI functions, like navigation, are connected to
      //    the content in the viewport, instead of the tool page.
      tunnel = tunnelToInnerBrowser(tab.linkedBrowser, innerBrowser);
      debug("Wait until tunnel start");
      await tunnel.start();

      // Swapping browsers disconnects the find bar UI from the browser.
      // If the find bar has been initialized, reconnect it.
      if (gBrowser.isFindBarInitialized(tab)) {
        const findBar = gBrowser.getCachedFindBar(tab);
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
      debug("Exit swap start");
    },

    stop() {
      // Hide the browser content temporarily while things move around to avoid displaying
      // strange intermediate states.
      tab.linkedBrowser.style.visibility = "hidden";

      // 1. Stop the tunnel between outer and inner browsers.
      tunnel.stop();
      tunnel = null;

      // 2. Create a temporary, hidden tab to hold the content.
      const contentTab = addTabSilently("about:blank", {
        skipAnimation: true,
        userContextId: tab.userContextId,
      });
      gBrowser.hideTab(contentTab);
      const contentBrowser = contentTab.linkedBrowser;

      // 3. Mark the content tab browser's docshell as active so the frame
      //    is created eagerly and will be ready to swap.
      contentBrowser.docShellIsActive = true;

      // 4. Swap tab content from the browser within the viewport in the tool UI
      //    to the regular browser tab, preserving all state via
      //    `gBrowser._swapBrowserDocShells`.
      dispatchDevToolsBrowserSwap(innerBrowser, contentBrowser);
      swapBrowserDocShells(contentTab, innerBrowser);
      innerBrowser = null;

      // Copy tab listener state flags to content tab.  See similar comment in `start`
      // above for more details.
      const stateFlags = gBrowser._tabListeners.get(tab).mStateFlags;
      gBrowser._tabListeners.get(contentTab).mStateFlags = stateFlags;

      // 5. Force the original browser tab to be remote since web content is
      //    loaded in the child process, and we're about to swap the content
      //    into this tab.
      gBrowser.updateBrowserRemoteness(tab.linkedBrowser, {
        remoteType: contentBrowser.remoteType,
      });

      // 6. Swap the content into the original browser tab and close the
      //    temporary tab used to hold the content via
      //    `swapBrowsersAndCloseOther`.
      dispatchDevToolsBrowserSwap(contentBrowser, tab.linkedBrowser);
      swapBrowsersAndCloseOtherSilently(tab, contentTab);

      // Swapping browsers disconnects the find bar UI from the browser.
      // If the find bar has been initialized, reconnect it.
      if (gBrowser.isFindBarInitialized(tab)) {
        const findBar = gBrowser.getCachedFindBar(tab);
        findBar.browser = tab.linkedBrowser;
        if (!findBar.hidden) {
          // Force the find bar to activate again, restoring the search string.
          findBar.onFindCommand();
        }
      }

      gBrowser = null;
      browserWindow = null;

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
const NAVIGATION_PROPERTIES = ["currentURI", "contentTitle", "securityUI"];

function freezeNavigationState(tab) {
  // Browser navigation properties we'll freeze temporarily to avoid "blinking" in the
  // location bar, etc. caused by the containerURL peeking through before the swap is
  // complete.
  for (const property of NAVIGATION_PROPERTIES) {
    const value = tab.linkedBrowser[property];
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
  for (const property of NAVIGATION_PROPERTIES) {
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
        return this.messageManager.remoteType;
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
  // swapped between browsers in browser.js's `swapDocShells`, and then their
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
      get progressListeners() {
        return [];
      },
    };
  }
  if (browser._remoteWebProgress == undefined) {
    browser._remoteWebProgress = {
      addProgressListener() {},
      removeProgressListener() {},
    };
  }
}

function tabLoaded(tab) {
  return new Promise(resolve => {
    function handle(event) {
      if (
        event.originalTarget != tab.linkedBrowser.contentDocument ||
        event.target.location.href == "about:blank"
      ) {
        return;
      }
      tab.linkedBrowser.removeEventListener("load", handle, true);
      resolve(event);
    }

    tab.linkedBrowser.addEventListener("load", handle, true);
  });
}

exports.swapToInnerBrowser = swapToInnerBrowser;
