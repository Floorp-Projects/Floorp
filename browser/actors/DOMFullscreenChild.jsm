/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["DOMFullscreenChild"];

class DOMFullscreenChild extends JSWindowActorChild {
  receiveMessage(aMessage) {
    let window = this.contentWindow;
    let windowUtils = window?.windowUtils;

    switch (aMessage.name) {
      case "DOMFullscreen:Entered": {
        if (!windowUtils) {
          // If we are not able to enter fullscreen, tell the parent to just
          // exit.
          this.sendAsyncMessage("DOMFullscreen:Exit", {});
          break;
        }

        let remoteFrameBC = aMessage.data.remoteFrameBC;
        if (remoteFrameBC) {
          let remoteFrame = remoteFrameBC.embedderElement;
          if (!remoteFrame) {
            // This could happen when the page navigate away and trigger a
            // process switching during fullscreen transition, tell the parent
            // to just exit.
            this.sendAsyncMessage("DOMFullscreen:Exit", {});
            break;
          }
          this._isNotTheRequestSource = true;
          windowUtils.remoteFrameFullscreenChanged(remoteFrame);
        } else {
          this._waitForMozAfterPaint = true;
          this._lastTransactionId = windowUtils.lastTransactionId;
          if (
            !windowUtils.handleFullscreenRequests() &&
            !this.document.fullscreenElement
          ) {
            // If we don't actually have any pending fullscreen request
            // to handle, neither we have been in fullscreen, tell the
            // parent to just exit.
            this.sendAsyncMessage("DOMFullscreen:Exit", {});
          }
        }
        break;
      }
      case "DOMFullscreen:CleanUp": {
        let isNotTheRequestSource = !!aMessage.data.remoteFrameBC;
        // If we've exited fullscreen at this point, no need to record
        // transaction id or call exit fullscreen. This is especially
        // important for pre-e10s, since in that case, it is possible
        // that no more paint would be triggered after this point.
        if (this.document.fullscreenElement) {
          this._isNotTheRequestSource = isNotTheRequestSource;
          // Need to wait for the MozAfterPaint after exiting fullscreen if
          // this is the request source.
          this._waitForMozAfterPaint = !this._isNotTheRequestSource;
          // windowUtils could be null if the associated window is not current
          // active window. In this case, document must be in the process of
          // exiting fullscreen, it is okay to not ask it to exit fullscreen.
          if (windowUtils) {
            this._lastTransactionId = windowUtils.lastTransactionId;
            windowUtils.exitFullscreen();
          }
        } else if (isNotTheRequestSource) {
          // If we are not the request source and have exited fullscreen, reply
          // Exited to parent as parent is waiting for our reply.
          this.sendAsyncMessage("DOMFullscreen:Exited", {});
        } else {
          // If we've already exited fullscreen, it is possible that no more
          // paint would be triggered, so don't wait for MozAfterPaint.
          // TODO: There might be some way to move this code around a bit to
          //       make it easier to follow. Somehow handle the "local" case in
          //       one place and the isNotTheRequestSource case after that.
          this.sendAsyncMessage("DOMFullscreen:Painted", {});
        }
        break;
      }
      case "DOMFullscreen:Painted": {
        Services.obs.notifyObservers(window, "fullscreen-painted");
        break;
      }
    }
  }

  handleEvent(aEvent) {
    if (this.hasBeenDestroyed()) {
      // Make sure that this actor is alive before going further because
      // if it's not the case, any attempt to send a message or access
      // objects such as 'contentWindow' will fail. (See bug 1590138)
      return;
    }

    switch (aEvent.type) {
      case "MozDOMFullscreen:Request": {
        this.sendAsyncMessage("DOMFullscreen:Request", {});
        break;
      }
      case "MozDOMFullscreen:NewOrigin": {
        this.sendAsyncMessage("DOMFullscreen:NewOrigin", {
          originNoSuffix: aEvent.target.nodePrincipal.originNoSuffix,
        });
        break;
      }
      case "MozDOMFullscreen:Exit": {
        this.sendAsyncMessage("DOMFullscreen:Exit", {});
        break;
      }
      case "MozDOMFullscreen:Entered":
      case "MozDOMFullscreen:Exited": {
        if (this._isNotTheRequestSource) {
          // Fullscreen change event for a frame in the
          // middle (content frame embedding the oop frame where the
          // request comes from)

          delete this._isNotTheRequestSource;
          this.sendAsyncMessage(aEvent.type.replace("Moz", ""), {});
          break;
        }

        if (this._waitForMozAfterPaint) {
          delete this._waitForMozAfterPaint;
          this._listeningWindow = this.contentWindow.windowRoot;
          this._listeningWindow.addEventListener("MozAfterPaint", this);
        }

        if (!this.document || !this.document.fullscreenElement) {
          // If we receive any fullscreen change event, and find we are
          // actually not in fullscreen, also ask the parent to exit to
          // ensure that the parent always exits fullscreen when we do.
          this.sendAsyncMessage("DOMFullscreen:Exit", {});
        }
        break;
      }
      case "MozAfterPaint": {
        // Only send Painted signal after we actually finish painting
        // the transition for the fullscreen change.
        // Note that this._lastTransactionId is not set when in pre-e10s
        // mode, so we need to check that explicitly.
        if (
          !this._lastTransactionId ||
          aEvent.transactionId > this._lastTransactionId
        ) {
          this._listeningWindow.removeEventListener("MozAfterPaint", this);
          delete this._listeningWindow;
          this.sendAsyncMessage("DOMFullscreen:Painted", {});
        }
        break;
      }
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
