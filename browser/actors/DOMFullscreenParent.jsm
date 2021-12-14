/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["DOMFullscreenParent"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

class DOMFullscreenParent extends JSWindowActorParent {
  waitingForChildFullscreen = false;

  updateFullscreenWindowReference(aWindow) {
    if (aWindow.document.documentElement.hasAttribute("inDOMFullscreen")) {
      this._fullscreenWindow = aWindow;
    } else {
      delete this._fullscreenWindow;
    }
  }

  didDestroy() {
    let window = this._fullscreenWindow;
    if (!window) {
      return;
    }

    if (this.waitingForChildFullscreen) {
      // We were killed while waiting for our DOMFullscreenChild
      // to transition to fullscreen so we abort the entire
      // fullscreen transition to prevent getting stuck in a
      // partial fullscreen state. We need to go through the
      // document since window.Fullscreen could be undefined
      // at this point.
      //
      // This could reject if we're not currently in fullscreen
      // so just ignore rejection.
      window.document.exitFullscreen().catch(() => {});
      return;
    }

    // Need to resume Chrome UI if the window is still in fullscreen UI
    // to avoid the window stays in fullscreen problem. (See Bug 1620341)
    if (window.document.documentElement.hasAttribute("inDOMFullscreen")) {
      if (window.FullScreen) {
        window.FullScreen.cleanupDomFullscreen(this);
      }
      if (window.windowUtils) {
        window.windowUtils.remoteFrameFullscreenReverted();
      }
    }
    this.updateFullscreenWindowReference(window);
  }

  receiveMessage(aMessage) {
    let topBrowsingContext = this.browsingContext.top;
    let browser = topBrowsingContext.embedderElement;

    if (!browser) {
      // No need to go further when the browser is not accessible anymore
      // (which can happen when the tab is closed for instance),
      return;
    }

    let window = browser.ownerGlobal;
    switch (aMessage.name) {
      case "DOMFullscreen:Request": {
        this.requestOrigin = this;
        this.addListeners(window);
        window.windowUtils.remoteFrameFullscreenChanged(browser);
        break;
      }
      case "DOMFullscreen:NewOrigin": {
        // Don't show the warning if we've already exited fullscreen.
        if (window.document.fullscreen) {
          window.PointerlockFsWarning.showFullScreen(
            aMessage.data.originNoSuffix
          );
        }
        break;
      }
      case "DOMFullscreen:Entered": {
        this.waitingForChildFullscreen = false;
        window.FullScreen.enterDomFullscreen(browser, this);
        this.updateFullscreenWindowReference(window);
        break;
      }
      case "DOMFullscreen:Exit": {
        window.windowUtils.remoteFrameFullscreenReverted();
        break;
      }
      case "DOMFullscreen:Exited": {
        window.FullScreen.cleanupDomFullscreen(this);
        this.updateFullscreenWindowReference(window);
        break;
      }
      case "DOMFullscreen:Painted": {
        Services.obs.notifyObservers(window, "fullscreen-painted");
        this.sendAsyncMessage("DOMFullscreen:Painted", {});
        TelemetryStopwatch.finish("FULLSCREEN_CHANGE_MS");
        break;
      }
    }
  }

  handleEvent(aEvent) {
    let window = aEvent.currentTarget.ownerGlobal;
    switch (aEvent.type) {
      case "MozDOMFullscreen:Entered": {
        // The event target is the element which requested the DOM
        // fullscreen. If we were entering DOM fullscreen for a remote
        // browser, the target would be the browser which was the parameter of
        // `remoteFrameFullscreenChanged` call. If the fullscreen
        // request was initiated from an in-process browser, we need
        // to get its corresponding browser here.
        let browser;
        if (aEvent.target.ownerGlobal == window) {
          browser = aEvent.target;
        } else {
          browser = aEvent.target.ownerGlobal.docShell.chromeEventHandler;
        }

        // Addon installation should be cancelled when entering fullscreen for security and usability reasons.
        // Installation prompts in fullscreen can trick the user into installing unwanted addons.
        // In fullscreen the notification box does not have a clear visual association with its parent anymore.
        if (window.gXPInstallObserver) {
          window.gXPInstallObserver.removeAllNotifications(browser);
        }

        TelemetryStopwatch.start("FULLSCREEN_CHANGE_MS");
        window.FullScreen.enterDomFullscreen(browser, this);
        this.updateFullscreenWindowReference(window);
        break;
      }
      case "MozDOMFullscreen:Exited": {
        TelemetryStopwatch.start("FULLSCREEN_CHANGE_MS");

        // Make sure that the actor has not been destroyed before
        // accessing its browsing context. Otherwise, a error may
        // occur and hence cleanupDomFullscreen not executed, resulting
        // in the browser window being in an unstable state.
        // (Bug 1590138).
        if (!this.hasBeenDestroyed() && !this.requestOrigin) {
          this.requestOrigin = this;
        }
        window.FullScreen.cleanupDomFullscreen(this);
        this.updateFullscreenWindowReference(window);
        this.removeListeners(window);
        break;
      }
    }
  }

  addListeners(aWindow) {
    aWindow.addEventListener(
      "MozDOMFullscreen:Entered",
      this,
      /* useCapture */ true,
      /* wantsUntrusted */
      false
    );
    aWindow.addEventListener(
      "MozDOMFullscreen:Exited",
      this,
      /* useCapture */ true,
      /* wantsUntrusted */ false
    );
  }

  removeListeners(aWindow) {
    aWindow.removeEventListener("MozDOMFullscreen:Entered", this, true);
    aWindow.removeEventListener("MozDOMFullscreen:Exited", this, true);
  }

  /**
   * Get the actor where the original fullscreen
   * enter or exit request comes from.
   */
  get requestOrigin() {
    let requestOrigin = this.browsingContext.top.fullscreenRequestOrigin;
    return requestOrigin && requestOrigin.get();
  }

  /**
   * Store the actor where the original fullscreen
   * enter or exit request comes from in the top level
   * browsing context.
   */
  set requestOrigin(aActor) {
    if (aActor) {
      this.browsingContext.top.fullscreenRequestOrigin = Cu.getWeakReference(
        aActor
      );
    } else {
      delete this.browsingContext.top.fullscreenRequestOrigin;
    }
  }

  hasBeenDestroyed() {
    // The 'didDestroy' callback is not always getting called.
    // So we can't rely on it here. Instead, we will try to access
    // the browsing context to judge wether the actor has
    // been destroyed or not.
    try {
      return !this.browsingContext;
    } catch {
      return true;
    }
  }
}
