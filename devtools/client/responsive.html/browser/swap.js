/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const promise = require("promise");
const { Task } = require("devtools/shared/task");

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

  return {

    start: Task.async(function* () {
      // 1. Create a temporary, hidden tab to load the tool UI.
      let containerTab = gBrowser.addTab(containerURL, {
        skipAnimation: true,
      });
      gBrowser.hideTab(containerTab);
      let containerBrowser = containerTab.linkedBrowser;

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
      gBrowser._swapBrowserDocShells(tab, innerBrowser);

      // 5. Force the original browser tab to be non-remote since the tool UI
      //    must be loaded in the parent process, and we're about to swap the
      //    tool UI into this tab.
      gBrowser.updateBrowserRemoteness(tab.linkedBrowser, false);

      // 6. Swap the tool UI (with viewport showing the content) into the
      //    original browser tab and close the temporary tab used to load the
      //    tool via `swapBrowsersAndCloseOther`.
      gBrowser.swapBrowsersAndCloseOther(tab, containerTab);
    }),

    stop() {
      // 1. Create a temporary, hidden tab to hold the content.
      let contentTab = gBrowser.addTab("about:blank", {
        skipAnimation: true,
      });
      gBrowser.hideTab(contentTab);
      let contentBrowser = contentTab.linkedBrowser;

      // 2. Mark the content tab browser's docshell as active so the frame
      //    is created eagerly and will be ready to swap.
      contentBrowser.docShellIsActive = true;

      // 3. Swap tab content from the browser within the viewport in the tool UI
      //    to the regular browser tab, preserving all state via
      //    `gBrowser._swapBrowserDocShells`.
      gBrowser._swapBrowserDocShells(contentTab, innerBrowser);
      innerBrowser = null;

      // 4. Force the original browser tab to be remote since web content is
      //    loaded in the child process, and we're about to swap the content
      //    into this tab.
      gBrowser.updateBrowserRemoteness(tab.linkedBrowser, true);

      // 5. Swap the content into the original browser tab and close the
      //    temporary tab used to hold the content via
      //    `swapBrowsersAndCloseOther`.
      gBrowser.swapBrowsersAndCloseOther(tab, contentTab);
      gBrowser = null;
    },

  };
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
  if (browser.messageManager == undefined) {
    Object.defineProperty(browser, "messageManager", {
      get() {
        return this.frameLoader.messageManager;
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
