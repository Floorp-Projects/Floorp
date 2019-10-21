/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["DOMFullscreenParent"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

class DOMFullscreenParent extends JSWindowActorParent {
  receiveMessage(aMessage) {
    let topBrowsingContext = this.browsingContext.top;
    let browser = topBrowsingContext.embedderElement;
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
        window.FullScreen.enterDomFullscreen(browser, this);
        break;
      }
      case "DOMFullscreen:Exit": {
        window.windowUtils.remoteFrameFullscreenReverted();
        break;
      }
      case "DOMFullscreen:Exited": {
        window.FullScreen.cleanupDomFullscreen(this);
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
    let topBrowsingContext = this.browsingContext.top;
    let window = topBrowsingContext.embedderElement.ownerGlobal;
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
        break;
      }
      case "MozDOMFullscreen:Exited": {
        TelemetryStopwatch.start("FULLSCREEN_CHANGE_MS");
        if (!this.requestOrigin) {
          this.requestOrigin = this;
        }
        window.FullScreen.cleanupDomFullscreen(this);
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
}
