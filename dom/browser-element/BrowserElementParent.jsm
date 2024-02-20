/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* BrowserElementParent injects script to listen for certain events in the
 * child.  We then listen to messages from the child script and take
 * appropriate action here in the parent.
 */

const { BrowserElementPromptService } = ChromeUtils.import(
  "resource://gre/modules/BrowserElementPromptService.jsm"
);

function debug(msg) {
  // dump("BrowserElementParent - " + msg + "\n");
}

function handleWindowEvent(e) {
  if (this._browserElementParents) {
    let beps = ChromeUtils.nondeterministicGetWeakMapKeys(
      this._browserElementParents
    );
    beps.forEach(bep => bep._handleOwnerEvent(e));
  }
}

function BrowserElementParent() {
  debug("Creating new BrowserElementParent object");
}

BrowserElementParent.prototype = {
  classDescription: "BrowserElementAPI implementation",
  classID: Components.ID("{9f171ac4-0939-4ef8-b360-3408aedc3060}"),
  contractID: "@mozilla.org/dom/browser-element-api;1",
  QueryInterface: ChromeUtils.generateQI([
    "nsIBrowserElementAPI",
    "nsISupportsWeakReference",
  ]),

  setFrameLoader(frameLoader) {
    debug("Setting frameLoader");
    this._frameLoader = frameLoader;
    this._frameElement = frameLoader.ownerElement;
    if (!this._frameElement) {
      debug("No frame element?");
      return;
    }
    // Listen to visibilitychange on the iframe's owner window, and forward
    // changes down to the child.  We want to do this while registering as few
    // visibilitychange listeners on _window as possible, because such a listener
    // may live longer than this BrowserElementParent object.
    //
    // To accomplish this, we register just one listener on the window, and have
    // it reference a WeakMap whose keys are all the BrowserElementParent objects
    // on the window.  Then when the listener fires, we iterate over the
    // WeakMap's keys (which we can do, because we're chrome) to notify the
    // BrowserElementParents.
    if (!this._window._browserElementParents) {
      this._window._browserElementParents = new WeakMap();
      let handler = handleWindowEvent.bind(this._window);
      let windowEvents = ["visibilitychange"];
      for (let event of windowEvents) {
        Services.els.addSystemEventListener(
          this._window,
          event,
          handler,
          /* useCapture = */ true
        );
      }
    }

    this._window._browserElementParents.set(this, null);

    // Insert ourself into the prompt service.
    BrowserElementPromptService.mapFrameToBrowserElementParent(
      this._frameElement,
      this
    );
    this._setupMessageListener();
  },

  destroyFrameScripts() {
    debug("Destroying frame scripts");
    this._mm.sendAsyncMessage("browser-element-api:destroy");
  },

  _setupMessageListener() {
    this._mm = this._frameLoader.messageManager;
    this._mm.addMessageListener("browser-element-api:call", this);
  },

  receiveMessage(aMsg) {
    if (!this._isAlive()) {
      return undefined;
    }

    // Messages we receive are handed to functions which take a (data) argument,
    // where |data| is the message manager's data object.
    // We use a single message and dispatch to various function based
    // on data.msg_name
    let mmCalls = {
      hello: this._recvHello,
    };

    let mmSecuritySensitiveCalls = {
      showmodalprompt: this._handleShowModalPrompt,
    };

    if (aMsg.data.msg_name in mmCalls) {
      return mmCalls[aMsg.data.msg_name].apply(this, arguments);
    } else if (aMsg.data.msg_name in mmSecuritySensitiveCalls) {
      return mmSecuritySensitiveCalls[aMsg.data.msg_name].apply(
        this,
        arguments
      );
    }
    return undefined;
  },

  _removeMessageListener() {
    this._mm.removeMessageListener("browser-element-api:call", this);
  },

  /**
   * You shouldn't touch this._frameElement or this._window if _isAlive is
   * false.  (You'll likely get an exception if you do.)
   */
  _isAlive() {
    return (
      !Cu.isDeadWrapper(this._frameElement) &&
      !Cu.isDeadWrapper(this._frameElement.ownerDocument) &&
      !Cu.isDeadWrapper(this._frameElement.ownerGlobal)
    );
  },

  get _window() {
    return this._frameElement.ownerGlobal;
  },

  _sendAsyncMsg(msg, data) {
    try {
      if (!data) {
        data = {};
      }

      data.msg_name = msg;
      this._mm.sendAsyncMessage("browser-element-api:call", data);
    } catch (e) {
      return false;
    }
    return true;
  },

  _recvHello() {
    debug("recvHello");

    // Inform our child if our owner element's document is invisible.  Note
    // that we must do so here, rather than in the BrowserElementParent
    // constructor, because the BrowserElementChild may not be initialized when
    // we run our constructor.
    if (this._window.document.hidden) {
      this._ownerVisibilityChange();
    }
  },

  /**
   * Fire either a vanilla or a custom event, depending on the contents of
   * |data|.
   */
  _fireEventFromMsg(data) {
    let detail = data.json;
    let name = detail.msg_name;

    // For events that send a "_payload_" property, we just want to transmit
    // this in the event.
    if ("_payload_" in detail) {
      detail = detail._payload_;
    }

    debug("fireEventFromMsg: " + name + ", " + JSON.stringify(detail));
    let evt = this._createEvent(name, detail, /* cancelable = */ false);
    this._frameElement.dispatchEvent(evt);
  },

  _handleShowModalPrompt(data) {
    // Fire a showmodalprmopt event on the iframe.  When this method is called,
    // the child is spinning in a nested event loop waiting for an
    // unblock-modal-prompt message.
    //
    // If the embedder calls preventDefault() on the showmodalprompt event,
    // we'll block the child until event.detail.unblock() is called.
    //
    // Otherwise, if preventDefault() is not called, we'll send the
    // unblock-modal-prompt message to the child as soon as the event is done
    // dispatching.

    let detail = data.json;
    debug("handleShowPrompt " + JSON.stringify(detail));

    // Strip off the windowID property from the object we send along in the
    // event.
    let windowID = detail.windowID;
    delete detail.windowID;
    debug("Event will have detail: " + JSON.stringify(detail));
    let evt = this._createEvent(
      "showmodalprompt",
      detail,
      /* cancelable = */ true
    );

    let self = this;
    let unblockMsgSent = false;
    function sendUnblockMsg() {
      if (unblockMsgSent) {
        return;
      }
      unblockMsgSent = true;

      // We don't need to sanitize evt.detail.returnValue (e.g. converting the
      // return value of confirm() to a boolean); Gecko does that for us.

      let data = { windowID, returnValue: evt.detail.returnValue };
      self._sendAsyncMsg("unblock-modal-prompt", data);
    }

    Cu.exportFunction(sendUnblockMsg, evt.detail, { defineAs: "unblock" });

    this._frameElement.dispatchEvent(evt);

    if (!evt.defaultPrevented) {
      // Unblock the inner frame immediately.  Otherwise we'll unblock upon
      // evt.detail.unblock().
      sendUnblockMsg();
    }
  },

  _createEvent(evtName, detail, cancelable) {
    // This will have to change if we ever want to send a CustomEvent with null
    // detail.  For now, it's OK.
    if (detail !== undefined && detail !== null) {
      detail = Cu.cloneInto(detail, this._window);
      return new this._window.CustomEvent("mozbrowser" + evtName, {
        bubbles: true,
        cancelable,
        detail,
      });
    }

    return new this._window.Event("mozbrowser" + evtName, {
      bubbles: true,
      cancelable,
    });
  },

  _handleOwnerEvent(evt) {
    switch (evt.type) {
      case "visibilitychange":
        this._ownerVisibilityChange();
        break;
    }
  },

  /**
   * Called when the visibility of the window which owns this iframe changes.
   */
  _ownerVisibilityChange() {
    let bc = this._frameLoader?.browsingContext;
    if (bc) {
      bc.isActive = !this._window.document.hidden;
    }
  },
};

var EXPORTED_SYMBOLS = ["BrowserElementParent"];
