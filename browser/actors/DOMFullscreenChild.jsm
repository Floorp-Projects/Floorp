/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["DOMFullscreenChild"];

const { ActorChild } = ChromeUtils.import(
  "resource://gre/modules/ActorChild.jsm"
);

class DOMFullscreenChild extends ActorChild {
  receiveMessage(aMessage) {
    let windowUtils = this.content && this.content.windowUtils;
    switch (aMessage.name) {
      case "DOMFullscreen:Entered": {
        this._lastTransactionId = windowUtils.lastTransactionId;
        if (
          !windowUtils.handleFullscreenRequests() &&
          !this.content.document.fullscreenElement
        ) {
          // If we don't actually have any pending fullscreen request
          // to handle, neither we have been in fullscreen, tell the
          // parent to just exit.
          this.mm.sendAsyncMessage("DOMFullscreen:Exit");
        }
        break;
      }
      case "DOMFullscreen:CleanUp": {
        // If we've exited fullscreen at this point, no need to record
        // transaction id or call exit fullscreen. This is especially
        // important for non-e10s, since in that case, it is possible
        // that no more paint would be triggered after this point.
        if (this.content.document.fullscreenElement && windowUtils) {
          this._lastTransactionId = windowUtils.lastTransactionId;
          windowUtils.exitFullscreen();
        }
        break;
      }
    }
  }

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "MozDOMFullscreen:Request": {
        this.mm.sendAsyncMessage("DOMFullscreen:Request");
        break;
      }
      case "MozDOMFullscreen:NewOrigin": {
        this.mm.sendAsyncMessage("DOMFullscreen:NewOrigin", {
          originNoSuffix: aEvent.target.nodePrincipal.originNoSuffix,
        });
        break;
      }
      case "MozDOMFullscreen:Exit": {
        this.mm.sendAsyncMessage("DOMFullscreen:Exit");
        break;
      }
      case "MozDOMFullscreen:Entered":
      case "MozDOMFullscreen:Exited": {
        this.mm.addEventListener("MozAfterPaint", this);
        if (!this.content || !this.content.document.fullscreenElement) {
          // If we receive any fullscreen change event, and find we are
          // actually not in fullscreen, also ask the parent to exit to
          // ensure that the parent always exits fullscreen when we do.
          this.mm.sendAsyncMessage("DOMFullscreen:Exit");
        }
        break;
      }
      case "MozAfterPaint": {
        // Only send Painted signal after we actually finish painting
        // the transition for the fullscreen change.
        // Note that this._lastTransactionId is not set when in non-e10s
        // mode, so we need to check that explicitly.
        if (
          !this._lastTransactionId ||
          aEvent.transactionId > this._lastTransactionId
        ) {
          this.mm.removeEventListener("MozAfterPaint", this);
          this.mm.sendAsyncMessage("DOMFullscreen:Painted");
        }
        break;
      }
    }
  }
}
