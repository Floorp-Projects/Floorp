/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-env mozilla/frame-script */

function debug(msg) {
  // dump("BrowserElementChildPreload - " + msg + "\n");
}

debug("loaded");

var BrowserElementIsReady;

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { BrowserElementPromptService } = ChromeUtils.import(
  "resource://gre/modules/BrowserElementPromptService.jsm"
);

function sendAsyncMsg(msg, data) {
  // Ensure that we don't send any messages before BrowserElementChild.js
  // finishes loading.
  if (!BrowserElementIsReady) {
    return;
  }

  if (!data) {
    data = {};
  }

  data.msg_name = msg;
  sendAsyncMessage("browser-element-api:call", data);
}

var LISTENED_EVENTS = [
  // This listens to unload events from our message manager, but /not/ from
  // the |content| window.  That's because the window's unload event doesn't
  // bubble, and we're not using a capturing listener.  If we'd used
  // useCapture == true, we /would/ hear unload events from the window, which
  // is not what we want!
  { type: "unload", useCapture: false, wantsUntrusted: false },
];

/**
 * The BrowserElementChild implements one half of <iframe mozbrowser>.
 * (The other half is, unsurprisingly, BrowserElementParent.)
 *
 * This script is injected into an <iframe mozbrowser> via
 * nsIMessageManager::LoadFrameScript().
 *
 * Our job here is to listen for events within this frame and bubble them up to
 * the parent process.
 */

var global = this;

function BrowserElementChild() {
  // Maps outer window id --> weak ref to window.  Used by modal dialog code.
  this._windowIDDict = {};

  this._init();
}

BrowserElementChild.prototype = {
  _init() {
    debug("Starting up.");

    BrowserElementPromptService.mapWindowToBrowserElementChild(content, this);

    this._shuttingDown = false;

    LISTENED_EVENTS.forEach(event => {
      addEventListener(
        event.type,
        this,
        event.useCapture,
        event.wantsUntrusted
      );
    });

    addMessageListener("browser-element-api:call", this);
  },

  /**
   * Shut down the frame's side of the browser API.  This is called when:
   *   - our BrowserChildGlobal starts to die
   *   - the content is moved to frame without the browser API
   * This is not called when the page inside |content| unloads.
   */
  destroy() {
    debug("Destroying");
    this._shuttingDown = true;

    BrowserElementPromptService.unmapWindowToBrowserElementChild(content);

    LISTENED_EVENTS.forEach(event => {
      removeEventListener(
        event.type,
        this,
        event.useCapture,
        event.wantsUntrusted
      );
    });

    removeMessageListener("browser-element-api:call", this);
  },

  handleEvent(event) {
    switch (event.type) {
      case "unload":
        this.destroy(event);
        break;
    }
  },

  receiveMessage(message) {
    let self = this;

    let mmCalls = {
      "unblock-modal-prompt": this._recvStopWaiting,
      "owner-visibility-change": this._recvOwnerVisibilityChange,
    };

    if (message.data.msg_name in mmCalls) {
      return mmCalls[message.data.msg_name].apply(self, arguments);
    }
    return undefined;
  },

  get _windowUtils() {
    return content.document.defaultView.windowUtils;
  },

  _tryGetInnerWindowID(win) {
    let utils = win.windowUtils;
    try {
      return utils.currentInnerWindowID;
    } catch (e) {
      return null;
    }
  },

  /**
   * Show a modal prompt.  Called by BrowserElementPromptService.
   */
  showModalPrompt(win, args) {
    let utils = win.windowUtils;

    args.windowID = {
      outer: utils.outerWindowID,
      inner: this._tryGetInnerWindowID(win),
    };
    sendAsyncMsg("showmodalprompt", args);

    let returnValue = this._waitForResult(win);

    if (
      args.promptType == "prompt" ||
      args.promptType == "confirm" ||
      args.promptType == "custom-prompt"
    ) {
      return returnValue;
    }
    return undefined;
  },

  /**
   * Spin in a nested event loop until we receive a unblock-modal-prompt message for
   * this window.
   */
  _waitForResult(win) {
    debug("_waitForResult(" + win + ")");
    let utils = win.windowUtils;

    let outerWindowID = utils.outerWindowID;
    let innerWindowID = this._tryGetInnerWindowID(win);
    if (innerWindowID === null) {
      // I have no idea what waiting for a result means when there's no inner
      // window, so let's just bail.
      debug("_waitForResult: No inner window. Bailing.");
      return undefined;
    }

    this._windowIDDict[outerWindowID] = Cu.getWeakReference(win);

    debug(
      "Entering modal state (outerWindowID=" +
        outerWindowID +
        ", " +
        "innerWindowID=" +
        innerWindowID +
        ")"
    );

    utils.enterModalState();

    // We'll decrement win.modalDepth when we receive a unblock-modal-prompt message
    // for the window.
    if (!win.modalDepth) {
      win.modalDepth = 0;
    }
    win.modalDepth++;
    let origModalDepth = win.modalDepth;

    debug("Nested event loop - begin");
    Services.tm.spinEventLoopUntil(() => {
      // Bail out of the loop if the inner window changed; that means the
      // window navigated.  Bail out when we're shutting down because otherwise
      // we'll leak our window.
      if (this._tryGetInnerWindowID(win) !== innerWindowID) {
        debug(
          "_waitForResult: Inner window ID changed " +
            "while in nested event loop."
        );
        return true;
      }

      return win.modalDepth !== origModalDepth || this._shuttingDown;
    });
    debug("Nested event loop - finish");

    if (win.modalDepth == 0) {
      delete this._windowIDDict[outerWindowID];
    }

    // If we exited the loop because the inner window changed, then bail on the
    // modal prompt.
    if (innerWindowID !== this._tryGetInnerWindowID(win)) {
      throw Components.Exception(
        "Modal state aborted by navigation",
        Cr.NS_ERROR_NOT_AVAILABLE
      );
    }

    let returnValue = win.modalReturnValue;
    delete win.modalReturnValue;

    if (!this._shuttingDown) {
      utils.leaveModalState();
    }

    debug(
      "Leaving modal state (outerID=" +
        outerWindowID +
        ", " +
        "innerID=" +
        innerWindowID +
        ")"
    );
    return returnValue;
  },

  _recvStopWaiting(msg) {
    let outerID = msg.json.windowID.outer;
    let innerID = msg.json.windowID.inner;
    let returnValue = msg.json.returnValue;
    debug(
      "recvStopWaiting(outer=" +
        outerID +
        ", inner=" +
        innerID +
        ", returnValue=" +
        returnValue +
        ")"
    );

    if (!this._windowIDDict[outerID]) {
      debug("recvStopWaiting: No record of outer window ID " + outerID);
      return;
    }

    let win = this._windowIDDict[outerID].get();

    if (!win) {
      debug("recvStopWaiting, but window is gone\n");
      return;
    }

    if (innerID !== this._tryGetInnerWindowID(win)) {
      debug("recvStopWaiting, but inner ID has changed\n");
      return;
    }

    debug("recvStopWaiting " + win);
    win.modalReturnValue = returnValue;
    win.modalDepth--;
  },

  /**
   * Called when the window which contains this iframe becomes hidden or
   * visible.
   */
  _recvOwnerVisibilityChange(data) {
    debug("Received ownerVisibilityChange: (" + data.json.visible + ")");
    var visible = data.json.visible;
    if (docShell && docShell.isActive !== visible) {
      docShell.isActive = visible;
    }
  },
};

var api = new BrowserElementChild();
