/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { DOCUMENT_EVENT },
} = require("resource://devtools/server/actors/resources/index.js");
const isEveryFrameTargetEnabled = Services.prefs.getBoolPref(
  "devtools.every-frame-target.enabled",
  false
);
const {
  WILL_NAVIGATE_TIME_SHIFT,
} = require("resource://devtools/server/actors/webconsole/listeners/document-events.js");

class ParentProcessDocumentEventWatcher {
  /**
   * Start watching, from the parent process, for DOCUMENT_EVENT's "will-navigate" event related to a given Watcher Actor.
   *
   * All other DOCUMENT_EVENT events are implemented from another watcher class, running in the content process.
   * Note that this other content process watcher will also emit one special edgecase of will-navigate
   * retlated to the iframe dropdown menu.
   *
   * We have to move listen for navigation in the parent to better handle bfcache navigations
   * and more generally all navigations which are initiated from the parent process.
   * 'bfcacheInParent' feature enabled many types of navigations to be controlled from the parent process.
   *
   * This was especially important to have this implementation in the parent
   * because the navigation event may be fired too late in the content process.
   * Leading to will-navigate being emitted *after* the new target we navigate to is notified to the client.
   *
   * @param WatcherActor watcherActor
   *        The watcher actor from which we should observe document event
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   */
  async watch(watcherActor, { onAvailable }) {
    this.watcherActor = watcherActor;
    this.onAvailable = onAvailable;

    // List of listeners keyed by innerWindowId.
    // Listeners are called as soon as we emitted the will-navigate
    // resource for the related WindowGlobal.
    this._onceWillNavigate = new Map();

    // Filter browsing contexts to only have the top BrowsingContext of each tree of BrowsingContexts…
    const topLevelBrowsingContexts = this.watcherActor
      .getAllBrowsingContexts()
      .filter(browsingContext => browsingContext.top == browsingContext);

    // Only register one WebProgressListener per BrowsingContext tree.
    // We will be notified about children BrowsingContext navigations/state changes via the top level BrowsingContextWebProgressListener,
    // and BrowsingContextWebProgress.browsingContext attribute will be updated dynamically everytime
    // we get notified about a child BrowsingContext.
    // Note that regular web page toolbox will only have one BrowsingContext tree, for the given tab.
    // But the Browser Toolbox will have many trees to listen to, one per top-level Window, and also one per tab,
    // as tabs's BrowsingContext context aren't children of their top level window!
    //
    // Also save the WebProgress and not the BrowsingContext because `BrowsingContext.webProgress` will be undefined in destroy(),
    // while it is still valuable to call `webProgress.removeProgressListener`. Otherwise events keeps being notified!!
    this.webProgresses = topLevelBrowsingContexts.map(
      browsingContext => browsingContext.webProgress
    );
    this.webProgresses.forEach(webProgress => {
      webProgress.addProgressListener(
        this,
        Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
      );
    });
  }

  /**
   * Wait for the emission of will-navigate for a given WindowGlobal
   *
   * @param Number innerWindowId
   *        WindowGlobal's id we want to track
   * @return Promise
   *         Resolves immediatly if the WindowGlobal isn't tracked by any target
   *         -or- resolve later, once the WindowGlobal navigates to another document
   *         and will-navigate has been emitted.
   */
  onceWillNavigateIsEmitted(innerWindowId) {
    // Only delay the target-destroyed event if the target is for BrowsingContext for which we will emit will-navigate
    const isTracked = this.webProgresses.find(
      webProgress =>
        webProgress.browsingContext.currentWindowGlobal.innerWindowId ==
        innerWindowId
    );
    if (isTracked) {
      return new Promise(resolve => {
        this._onceWillNavigate.set(innerWindowId, resolve);
      });
    }
    return Promise.resolve();
  }

  onStateChange(progress, request, flag, status) {
    const isStart = flag & Ci.nsIWebProgressListener.STATE_START;
    const isDocument = flag & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT;
    if (isDocument && isStart) {
      const { browsingContext } = progress;
      // Ignore navigation for same-process iframes when EFT is disabled
      if (
        !browsingContext.currentWindowGlobal.isProcessRoot &&
        !isEveryFrameTargetEnabled
      ) {
        return;
      }
      // Ignore if we are still on the initial document,
      // as that's the navigation from it (about:blank) to the actual first location.
      // The target isn't created yet.
      if (browsingContext.currentWindowGlobal.isInitialDocument) {
        return;
      }

      // Only emit will-navigate for top-level targets.
      if (
        this.watcherActor.sessionContext.type == "all" &&
        browsingContext.isContent
      ) {
        // Never emit will-navigate for content browsing contexts in the Browser Toolbox.
        // They might verify `browsingContext.top == browsingContext` because of the chrome/content
        // boundary, but they do not represent a top-level target for this DevTools session.
        return;
      }
      const isTopLevel = browsingContext.top == browsingContext;
      if (!isTopLevel) {
        return;
      }

      const newURI = request instanceof Ci.nsIChannel ? request.URI.spec : null;
      const { innerWindowId } = browsingContext.currentWindowGlobal;
      this.onAvailable([
        {
          browsingContextID: browsingContext.id,
          innerWindowId,
          resourceType: DOCUMENT_EVENT,
          name: "will-navigate",
          time: Date.now() - WILL_NAVIGATE_TIME_SHIFT,
          isFrameSwitching: false,
          newURI,
        },
      ]);
      const callback = this._onceWillNavigate.get(innerWindowId);
      if (callback) {
        this._onceWillNavigate.delete(innerWindowId);
        callback();
      }
    }
  }

  get QueryInterface() {
    return ChromeUtils.generateQI([
      "nsIWebProgressListener",
      "nsISupportsWeakReference",
    ]);
  }

  destroy() {
    this.webProgresses.forEach(webProgress => {
      webProgress.removeProgressListener(
        this,
        Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
      );
    });
    this.webProgresses = null;
  }
}

module.exports = ParentProcessDocumentEventWatcher;
