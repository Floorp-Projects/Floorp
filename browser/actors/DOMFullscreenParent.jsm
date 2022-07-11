/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["DOMFullscreenParent"];

class DOMFullscreenParent extends JSWindowActorParent {
  // These properties get set by browser-fullScreenAndPointerLock.js.
  // TODO: Bug 1743703 - Consider moving the messaging component of
  //       browser-fullScreenAndPointerLock.js into the actor
  waitingForChildEnterFullscreen = false;
  waitingForChildExitFullscreen = false;
  // Cache the next message recipient actor and in-process browsing context that
  // is computed by _getNextMsgRecipientActor() of
  // browser-fullScreenAndPointerLock.js, this is used to ensure the fullscreen
  // cleanup messages goes the same route as fullscreen request, especially for
  // the cleanup that happens after actor is destroyed.
  // TODO: Bug 1743703 - Consider moving the messaging component of
  //       browser-fullScreenAndPointerLock.js into the actor
  nextMsgRecipient = null;

  updateFullscreenWindowReference(aWindow) {
    if (aWindow.document.documentElement.hasAttribute("inDOMFullscreen")) {
      this._fullscreenWindow = aWindow;
    } else {
      delete this._fullscreenWindow;
    }
  }

  cleanupDomFullscreen(aWindow) {
    if (!aWindow.FullScreen) {
      return;
    }

    // If we don't need to wait for child reply, i.e. cleanupDomFullscreen
    // doesn't message to child, and we've exit the fullscreen, there won't be
    // DOMFullscreen:Painted message from child and it is possible that no more
    // paint would be triggered, so just notify fullscreen-painted observer.
    if (
      !aWindow.FullScreen.cleanupDomFullscreen(this) &&
      !aWindow.document.fullscreen
    ) {
      Services.obs.notifyObservers(aWindow, "fullscreen-painted");
    }
  }

  /**
   * Clean up fullscreen state and resume chrome UI if window is in fullscreen
   * and this actor is the one where the original fullscreen enter or
   * exit request comes.
   */
  _cleanupFullscreenStateAndResumeChromeUI(aWindow) {
    this.cleanupDomFullscreen(aWindow);
    if (this.requestOrigin == this && aWindow.document.fullscreen) {
      aWindow.windowUtils.remoteFrameFullscreenReverted();
    }
  }

  didDestroy() {
    this._didDestroy = true;

    let window = this._fullscreenWindow;
    if (!window) {
      let topBrowsingContext = this.browsingContext.top;
      let browser = topBrowsingContext.embedderElement;
      if (!browser) {
        return;
      }

      if (
        this.waitingForChildExitFullscreen ||
        this.waitingForChildEnterFullscreen
      ) {
        this.waitingForChildExitFullscreen = false;
        this.waitingForChildEnterFullscreen = false;
        // We were destroyed while waiting for our DOMFullscreenChild to exit
        // or enter fullscreen, run cleanup steps anyway.
        this._cleanupFullscreenStateAndResumeChromeUI(browser.ownerGlobal);
      }
      return;
    }

    if (this.waitingForChildEnterFullscreen) {
      this.waitingForChildEnterFullscreen = false;
      if (window.document.fullscreen) {
        // We were destroyed while waiting for our DOMFullscreenChild
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
      this.cleanupDomFullscreen(window);
    }

    // Need to resume Chrome UI if the window is still in fullscreen UI
    // to avoid the window stays in fullscreen problem. (See Bug 1620341)
    if (window.document.documentElement.hasAttribute("inDOMFullscreen")) {
      this.cleanupDomFullscreen(window);
      if (window.windowUtils) {
        window.windowUtils.remoteFrameFullscreenReverted();
      }
    } else if (this.waitingForChildExitFullscreen) {
      this.waitingForChildExitFullscreen = false;
      // We were destroyed while waiting for our DOMFullscreenChild to exit
      // run cleanup steps anyway.
      this._cleanupFullscreenStateAndResumeChromeUI(window);
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
        this.waitingForChildExitFullscreen = false;
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
        this.updateFullscreenWindowReference(window);
        break;
      }
      case "DOMFullscreen:Entered": {
        this.nextMsgRecipient = null;
        this.waitingForChildEnterFullscreen = false;
        window.FullScreen.enterDomFullscreen(browser, this);
        this.updateFullscreenWindowReference(window);
        break;
      }
      case "DOMFullscreen:Exit": {
        this.waitingForChildEnterFullscreen = false;
        window.windowUtils.remoteFrameFullscreenReverted();
        break;
      }
      case "DOMFullscreen:Exited": {
        this.waitingForChildExitFullscreen = false;
        this.cleanupDomFullscreen(window);
        this.updateFullscreenWindowReference(window);
        break;
      }
      case "DOMFullscreen:Painted": {
        this.waitingForChildExitFullscreen = false;
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
        this.cleanupDomFullscreen(window);
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
    if (this._didDestroy) {
      return true;
    }

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
